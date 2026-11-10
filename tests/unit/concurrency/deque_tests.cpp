
#include <gtest/gtest.h>
#include <thread>
#include "concurrency/deque.h++"
TEST(DequeTests, BasicLogic) {
    Deque<size_t> deque;
    ASSERT_TRUE(deque.empty());
    deque.push(new size_t(100));
    ASSERT_FALSE(deque.empty());
    delete deque.pop();
    ASSERT_TRUE(deque.empty());
}
TEST(DequeTests, PushPop) {
}