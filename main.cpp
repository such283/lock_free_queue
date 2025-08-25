#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <set>
#include <mutex>
#include "queue.h" // Include your header file

class TestResults {
public:
    std::atomic<int> items_pushed{0};
    std::atomic<int> items_popped{0};
    std::atomic<int> successful_pops{0};
    std::atomic<int> empty_pops{0};
    std::set<int> popped_values;
    std::mutex results_mutex;

    void record_pop(int value) {
        std::lock_guard<std::mutex> lock(results_mutex);
        popped_values.insert(value);
    }

    void print_summary() {
        std::cout << "\n=== Test Results Summary ===" << std::endl;
        std::cout << "Items pushed: " << items_pushed.load() << std::endl;
        std::cout << "Items popped: " << items_popped.load() << std::endl;
        std::cout << "Successful pops: " << successful_pops.load() << std::endl;
        std::cout << "Empty pops: " << empty_pops.load() << std::endl;
        std::cout << "Unique values popped: " << popped_values.size() << std::endl;
    }
};

// Test: Multiple producers, multiple consumers (MPMC)
void test_multiple_producers_consumers() {
    std::cout << "\n--- MPMC Test: Multiple Producers/Multiple Consumers ---" << std::endl;
    lock_free_queue<int> queue;
    TestResults results;

    const int num_producers = 4;
    const int num_consumers = 4;
    const int items_per_producer = 2500;
    const int total_items = num_producers * items_per_producer;

    std::atomic<int> items_consumed{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    std::cout << "Starting " << num_producers << " producers and " << num_consumers << " consumers" << std::endl;
    std::cout << "Each producer will push " << items_per_producer << " items" << std::endl;
    std::cout << "Total expected items: " << total_items << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&queue, &results, p, items_per_producer]() {
            int start = p * items_per_producer;
            for (int i = 0; i < items_per_producer; ++i) {
                queue.push(start + i);
                results.items_pushed.fetch_add(1);
                // Add small random delay to create more contention
                if (i % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            }
            std::cout << "Producer " << p << " finished" << std::endl;
        });
    }

    // Create consumer threads
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&queue, &results, &items_consumed, total_items, c]() {
            int local_consumed = 0;
            while (items_consumed.load() < total_items) {
                auto result = queue.pop();
                if (result) {
                    results.record_pop(*result);
                    results.successful_pops.fetch_add(1);
                    items_consumed.fetch_add(1);
                    local_consumed++;
                } else {
                    results.empty_pops.fetch_add(1);
                    std::this_thread::yield();
                }
                results.items_popped.fetch_add(1);
            }
            std::cout << "Consumer " << c << " finished, consumed " << local_consumed << " items" << std::endl;
        });
    }

    // Wait for all producers to finish
    for (auto& producer : producers) {
        producer.join();
    }
    std::cout << "All producers finished" << std::endl;

    // Wait for all consumers to finish
    for (auto& consumer : consumers) {
        consumer.join();
    }
    std::cout << "All consumers finished" << std::endl;

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    results.print_summary();

    // Verification
    assert(results.successful_pops.load() == total_items);
    assert(results.popped_values.size() == total_items);

    std::cout << "✓ All items successfully processed by multiple consumers" << std::endl;
    std::cout << "Test completed in " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << (total_items * 1000.0 / duration.count()) << " operations/second" << std::endl;
}

int main() {
    std::cout << "Testing Lock-Free Queue Implementation - MPMC Test Only" << std::endl;
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads" << std::endl;

    try {
        test_multiple_producers_consumers();
        std::cout << "\n MPMC test passed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}