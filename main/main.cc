// *************************************
// ESP-IDF Task Concurrency for Addition
// 'main.cc'
// Author: jcjuarez
// *************************************

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <limits>
#include <vector>
#include <ctime>

//
// Not task-safe async result container.
// Must be associated only to one task of execution.
//
struct async_computational_result
{
    async_computational_result(
        const std::vector<std::uint8_t>& p_list,
        const std::uint16_t p_start_index,
        const std::uint16_t p_end_index,
        const std::uint8_t p_task_index)
        : m_is_ready(false),
          m_result(0),
          m_list(&p_list),
          m_start_index(p_start_index),
          m_end_index(p_end_index),
          m_task_index(p_task_index)
    {}

    //
    // Determines if the result is ready to be consumed.
    //
    bool m_is_ready;

    //
    // The ccalculated result. Only to be consumed after the associated flag indicates so.
    //
    std::uint32_t m_result;

    //
    // Handle to the list of numbers to add up.
    //
    const std::vector<std::uint8_t>* m_list;

    //
    // Starting index for computation.
    //
    const std::uint16_t m_start_index;

    //
    // Ending index for computation.
    //
    const std::uint16_t m_end_index;

    //
    // Index of the task executing the computation.
    //
    const std::uint8_t m_task_index;
};

//
// Adds up a set of values within a specified range.
//
void addition_task_worker(void* p_context)
{
    async_computational_result& async_result = *(reinterpret_cast<async_computational_result*>(p_context));
    const std::vector<std::uint8_t>& list = *(async_result.m_list);

    for (std::uint16_t index {async_result.m_start_index}; index < async_result.m_end_index; ++index)
    {
        ESP_LOGI("addition_task_worker", "Task[%d] The item is: %d", async_result.m_task_index, list[index]);
        async_result.m_result += list[index];
    }

    async_result.m_is_ready = true;

    vTaskDelete(nullptr);
}

extern "C" void app_main()
{
    // Seed the random number generator
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::uint8_t min_value {1};
    std::uint8_t max_value {std::numeric_limits<uint8_t>::max()};

    // Number of tasks for concurrent execution and list size.
    constexpr std::uint8_t number_tasks {10};
    constexpr std::uint16_t list_size {1'000};
    constexpr std::uint16_t items_per_task {list_size / number_tasks};

    for (;;)
    {
        std::vector<std::uint8_t> list(list_size);

        for (std::uint8_t& item : list)
        {
            item = min_value + std::rand() % (max_value - min_value + 1);
        }

        std::vector<std::unique_ptr<async_computational_result>> async_results;

        for (std::uint8_t task_index {0}; task_index < number_tasks; ++task_index)
        {
            const std::uint16_t start_index = task_index * items_per_task;
            const std::uint16_t end_index = start_index + items_per_task;

            ESP_LOGI("app_main", "Dispatching task for execution.");
            async_results.emplace_back(std::make_unique<async_computational_result>(
                list,
                start_index,
                end_index,
                task_index));

            xTaskCreate(
                addition_task_worker,
                nullptr,
                4096,
                reinterpret_cast<void*>(async_results.back().get()),
                tskIDLE_PRIORITY,
                nullptr);
        }

        for (;;)
        {
            // Busy waiting; this is spinning the associated CPU core.
            std::uint8_t number_tasks_completed {0};

            for (auto& async_result : async_results)
            {
                if (async_result->m_is_ready) ++number_tasks_completed;
            }

            if (number_tasks_completed == number_tasks) break;

            // Yield execution, otherwise watch dog might fire.
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        ESP_LOGI("app_main", "Tasks have finished execution.");

        std::uint32_t result {0};
        for (auto& async_result : async_results)
        {
            result += async_result->m_result;
        }

        ESP_LOGI("app_main", "Final result is: %ld", result);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
