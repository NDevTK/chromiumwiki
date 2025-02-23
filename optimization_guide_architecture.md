# Optimization Guide Architecture

This page documents the architecture of the Chrome Optimization Guide system.

## Overview

The Optimization Guide system in Chrome provides predictions and hints to improve browser performance and user experience. It fetches optimization hints from a remote service, caches them locally, and uses them to make decisions about applying optimizations for various features.

## Key Components

The Optimization Guide system consists of several key components that work together:

- **`OptimizationGuideDecider` (Abstract Interface):**
    - Defines the abstract interface for making optimization decisions.
    - Consumers (e.g., Chrome features) use `OptimizationGuideDecider` to query if an optimization can be applied for a given URL and optimization type.
    - Header file: `components/optimization_guide/core/optimization_guide_decider.h`

- **`OptimizationGuideKeyedService` (Concrete Implementation for Chrome):**
    - Concrete implementation of `OptimizationGuideDecider` for Chrome.
    - Implemented in `chrome/browser/optimization_guide/optimization_guide_keyed_service.h` and `chrome/browser/optimization_guide/optimization_guide_keyed_service.cc`.
    - A KeyedService associated with a Chrome profile.
    - Aggregates hint management and model execution functionalities.
    - Acts as a facade, delegating core hint management to `ChromeHintsManager`.

- **`ChromeHintsManager` (Concrete Implementation for Chrome):**
    - Chrome's concrete implementation of `HintsManager`.
    - Implemented in `chrome/browser/optimization_guide/chrome_hints_manager.h` and `chrome/browser/optimization_guide/chrome_hints_manager.cc`.
    - Manages hint fetching, caching, and filtering.
    - Performs the core hint-based decision logic.
    - Uses `HintCache` for storage and `HintsFetcher` for fetching hints.

- **`HintsManager` (Abstract Base Class):**
    - Base class for hint managers.
    - Defines core hint management functionalities, including hint caching, fetching, and optimization filtering.
    - Header file: `components/optimization_guide/core/hints_manager.h`

- **`HintCache`:**
    - Responsible for storing and retrieving optimization hints.
    - Part of `HintsManager`.

- **`HintsFetcher`:**
    - Fetches optimization hints from the remote Optimization Guide service.
    - Created and used by `HintsManager`.

- **`OptimizationFilter`:**
    - Manages optimization allowlists and blocklists for different optimization types.
    - Used by `HintsManager` to filter optimization decisions.

## Optimization Decision Flow

The optimization decision flow typically involves these steps:

1. **Consumer Request:** A Chrome feature requests an optimization decision by calling `OptimizationGuideKeyedService::CanApplyOptimization`.
2. **Delegation to `ChromeHintsManager`:** `OptimizationGuideKeyedService` delegates the request to its `hints_manager_` instance (`ChromeHintsManager`).
3. **`HintsManager` Decision:** `ChromeHintsManager` performs the actual decision-making process:
    - Retrieves hints from `HintCache`.
    - Checks optimization filters using `OptimizationFilter`.
    - Determines the `OptimizationTypeDecision`.
4. **Decision Conversion (Synchronous):** For synchronous `CanApplyOptimization` calls, `OptimizationGuideKeyedService` converts the internal `OptimizationTypeDecision` to the public `OptimizationGuideDecision`.
5. **Callback Invocation (Asynchronous):** For asynchronous `CanApplyOptimization` calls, `ChromeHintsManager` invokes the provided callback with the decision.
6. **Decision Return:** `OptimizationGuideKeyedService` returns the `OptimizationGuideDecision` to the consumer (synchronous) or the callback is invoked (asynchronous).

## Implementation Details

- `OptimizationGuideKeyedService` in `chrome/browser/optimization_guide/optimization_guide_keyed_service.cc` is the concrete implementation of `OptimizationGuideDecider` for Chrome.
- It inherits from `OptimizationGuideDecider` and overrides its methods.
- It creates and uses instances of `ChromeHintsManager` and `PredictionManager`.
- It delegates hint-related tasks to `ChromeHintsManager`, acting as a facade.

This page provides a high-level overview of the Optimization Guide architecture and its key components. For more details, refer to the source code and individual component documentation.