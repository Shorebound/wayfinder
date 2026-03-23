#include "core/Result.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <type_traits>

namespace Wayfinder::Tests
{
    using Wayfinder::Error;
    using Wayfinder::MakeError;
    using Wayfinder::Result;

    // ── Error type ───────────────────────────────────────────

    TEST_CASE("Error default-constructs with empty message")
    {
        const Error err;
        CHECK(err.GetMessage().empty());
    }

    TEST_CASE("Error constructed from std::string")
    {
        const Error err(std::string{"something went wrong"});
        CHECK(err.GetMessage() == "something went wrong");
    }

    TEST_CASE("Error constructed from C-string literal")
    {
        const Error err("file not found");
        CHECK(err.GetMessage() == "file not found");
    }

    TEST_CASE("Error converts to string_view")
    {
        const Error err("test message");
        const std::string_view sv = err;
        CHECK(sv == "test message");
    }

    TEST_CASE("Error equality comparison")
    {
        CHECK(Error("a") == Error("a"));
        CHECK_FALSE(Error("a") == Error("b"));
    }

    TEST_CASE("Error constructed from nullptr treats it as empty string")
    {
        const Error err(static_cast<const char*>(nullptr));
        CHECK(err.GetMessage().empty());
    }

    // ── Result<T> success path ───────────────────────────────

    TEST_CASE("Result<int> holds a value on success")
    {
        Result<int> r = 42;
        REQUIRE(r.has_value());
        CHECK(r.value() == 42);
    }

    TEST_CASE("Result<std::string> holds a value on success")
    {
        Result<std::string> r = std::string{"hello"};
        REQUIRE(r.has_value());
        CHECK(*r == "hello");
    }

    TEST_CASE("Result<void> success via default construction")
    {
        Result<void> r{};
        CHECK(r.has_value());
    }

    // ── Result<T> error path ─────────────────────────────────

    TEST_CASE("Result<int> holds an error on failure")
    {
        Result<int> r = MakeError("bad input");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().GetMessage() == "bad input");
    }

    TEST_CASE("Result<void> holds an error on failure")
    {
        Result<void> r = MakeError("init failed");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().GetMessage() == "init failed");
    }

    // ── MakeError factory ────────────────────────────────────

    TEST_CASE("MakeError from std::string")
    {
        auto e = MakeError(std::string{"dynamic"});
        Result<int> r = e;
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().GetMessage() == "dynamic");
    }

    TEST_CASE("MakeError from C-string")
    {
        Result<int> r = MakeError("literal");
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().GetMessage() == "literal");
    }

    // ── Bool-style usage ─────────────────────────────────────

    TEST_CASE("Result<void> is truthy on success")
    {
        Result<void> r{};
        CHECK(static_cast<bool>(r));
    }

    TEST_CASE("Result<void> is falsy on error")
    {
        Result<void> r = MakeError("nope");
        CHECK_FALSE(static_cast<bool>(r));
    }

    // ── Return-from-function pattern ─────────────────────────

    auto Divide(int a, int b) -> Result<int>
    {
        if (b == 0) return MakeError("division by zero");
        return a / b;
    }

    TEST_CASE("Result propagates through function return")
    {
        auto ok = Divide(10, 2);
        REQUIRE(ok.has_value());
        CHECK(*ok == 5);

        auto bad = Divide(10, 0);
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error().GetMessage() == "division by zero");
    }

    // ── value_or convenience ─────────────────────────────────

    TEST_CASE("Result value_or returns value on success")
    {
        Result<int> r = 7;
        CHECK(r.value_or(-1) == 7);
    }

    TEST_CASE("Result value_or returns default on error")
    {
        Result<int> r = MakeError("fail");
        CHECK(r.value_or(-1) == -1);
    }

    // ── Move semantics ───────────────────────────────────────

    TEST_CASE("Result<std::string> supports move semantics")
    {
        Result<std::string> r = std::string{"movable"};
        std::string taken = std::move(*r);
        CHECK(taken == "movable");
    }

    // ── Custom error type ────────────────────────────────────

    enum class CustomError
    {
        NotFound,
        Timeout,
    };

    TEST_CASE("Result with custom error type")
    {
        Result<int, CustomError> r = std::unexpected(CustomError::NotFound);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error() == CustomError::NotFound);
    }

    // ── Type traits ──────────────────────────────────────────

    TEST_CASE("Result<T> is the same as std::expected<T, Error>")
    {
        static_assert(std::is_same_v<Result<int>, std::expected<int, Error>>);
        static_assert(std::is_same_v<Result<void>, std::expected<void, Error>>);
        static_assert(std::is_same_v<Result<int, CustomError>, std::expected<int, CustomError>>);
    }

} // anonymous namespace
