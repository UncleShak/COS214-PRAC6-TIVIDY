#include <gtest/gtest.h>
#include <stdexcept>
#include "../src/WorkItem.h"
#include "../src/CreatedState.h"
#include "../src/AvailableState.h"
#include "../src/AssignedState.h"
#include "../src/StartedState.h"
#include "../src/CompletedState.h"

// Test 1: Verify the complete "Happy Path" transition sequence[cite: 23]
TEST(LifecycleTests, ValidTransitionsAdvanceState) {
    WorkItem task("Deliver Parcel");
    
    // Starts in Created
    EXPECT_EQ(task.getCurrentState()->getStateName(), "Created");

    // Created -> Available
    EXPECT_NO_THROW(task.makeAvailable());
    EXPECT_EQ(task.getCurrentState()->getStateName(), "Available");

    // Available -> Assigned
    EXPECT_NO_THROW(task.assign());
    EXPECT_EQ(task.getCurrentState()->getStateName(), "Assigned");

    // Assigned -> Started
    EXPECT_NO_THROW(task.start());
    EXPECT_EQ(task.getCurrentState()->getStateName(), "Started");

    // Started -> Completed
    EXPECT_NO_THROW(task.complete());
    EXPECT_EQ(task.getCurrentState()->getStateName(), "Completed");
}

// Test 2: Verify the engine strictly blocks illegal transitions[cite: 23, 24]
TEST(LifecycleTests, InvalidTransitionsThrowLogicError) {
    WorkItem task("Review Application");
    
    // Cannot complete a task that was just created
    EXPECT_THROW(task.complete(), std::logic_error);

    // Cannot start a task before it is made available and assigned
    EXPECT_THROW(task.start(), std::logic_error);

    // Make it available
    task.makeAvailable();

    // Still cannot complete it from the Available state
    EXPECT_THROW(task.complete(), std::logic_error);
}