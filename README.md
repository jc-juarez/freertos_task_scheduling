# Bare metal FreeRTOS task concurrency addition processing execution through ESP-IDF ❄️☄️

This is a simple implementation of an algorithm of task parallelism for adding numbers in a container through different tasks of execution. This is running on bare metal, so this is not multithreading but rather multitasking on top of the FreeRTOS kernel with ESP-IDF management facilities and access to the overloaded C++ standard library implementation on top of FreeRTOS bare metal memory management semantics.
