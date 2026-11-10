//
// Created by arthur
//

#include <atomic>
#include <sstream>
#include <thread>
#include <vector>

#include "Concerto/PackageGenerator/Plugin/FlatAttributes.hpp"
#include "Concerto/PackageGenerator/Plugin/PluginApi.h"
#include "Concerto/PackageGenerator/Plugin/ReflectionGeneratorPlugin.hpp"
#include <catch2/catch_test_macros.hpp>
#include <toml11/parser.hpp>

SCENARIO("GenerationContext write operations")
{
	using namespace cct;

	GIVEN("A GenerationContext with a valid output stream")
	{
		std::ostringstream stream;
		std::size_t indentLevel = 0;

		GenerationContext ctx;
		ctx.outputStream = &stream;
		ctx.indentLevel = &indentLevel;

		WHEN("Write is called with a simple string")
		{
			ctx.Write("Hello World");

			THEN("The string is written with a newline")
			{
				CHECK(stream.str() == "Hello World\n");
			}
		}

		WHEN("Write is called with format arguments")
		{
			ctx.Write("Value: {}", 42);

			THEN("The formatted string is written")
			{
				CHECK(stream.str() == "Value: 42\n");
			}
		}

		WHEN("NewLine is called")
		{
			ctx.NewLine();

			THEN("A newline is added")
			{
				CHECK(stream.str() == "\n");
			}
		}

		WHEN("EnterScope is called")
		{
			ctx.EnterScope();

			THEN("An opening brace is written and indent level increases")
			{
				CHECK(stream.str() == "{\n");
				CHECK(indentLevel == 1);
			}
		}

		WHEN("LeaveScope is called after EnterScope")
		{
			ctx.EnterScope();
			stream.str("");
			ctx.LeaveScope();

			THEN("A closing brace is written and indent level decreases")
			{
				CHECK(stream.str() == "}\n");
				CHECK(indentLevel == 0);
			}
		}

		WHEN("LeaveScope is called with a suffix")
		{
			ctx.EnterScope();
			stream.str("");
			ctx.LeaveScope(";");

			THEN("The closing brace has the suffix")
			{
				CHECK(stream.str() == "};\n");
			}
		}

		WHEN("Writing with indentation")
		{
			ctx.EnterScope();
			stream.str("");
			ctx.Write("indented line");

			THEN("The line has proper indentation")
			{
				CHECK(stream.str() == "\tindented line\n");
			}
		}

		WHEN("Multiple nested scopes")
		{
			ctx.EnterScope();
			ctx.EnterScope();
			stream.str("");
			ctx.Write("double indented");

			THEN("The line has double indentation")
			{
				CHECK(stream.str() == "\t\tdouble indented\n");
				CHECK(indentLevel == 2);
			}
		}
	}

	GIVEN("A GenerationContext with null pointers")
	{
		GenerationContext ctx;
		ctx.outputStream = nullptr;
		ctx.indentLevel = nullptr;

		WHEN("Write is called")
		{
			THEN("It does not crash")
			{
				CHECK_NOTHROW(ctx.Write("test"));
			}
		}

		WHEN("NewLine is called")
		{
			THEN("It does not crash")
			{
				CHECK_NOTHROW(ctx.NewLine());
			}
		}

		WHEN("EnterScope is called")
		{
			THEN("It does not crash")
			{
				CHECK_NOTHROW(ctx.EnterScope());
			}
		}

		WHEN("LeaveScope is called")
		{
			THEN("It does not crash")
			{
				CHECK_NOTHROW(ctx.LeaveScope());
			}
		}
	}
}

SCENARIO("C API GenerationContext functions")
{
	GIVEN("A GenerationContext cast to C API type")
	{
		std::ostringstream stream;
		std::size_t indentLevel = 0;

		cct::GenerationContext ctx;
		ctx.outputStream = &stream;
		ctx.indentLevel = &indentLevel;

		auto* cCtx = reinterpret_cast<CrpGenerationContext*>(&ctx);

		WHEN("crpGenerationContextWrite is called with simple string")
		{
			crpGenerationContextWrite(cCtx, "Hello from C API");

			THEN("The string is written with a newline")
			{
				CHECK(stream.str() == "Hello from C API\n");
			}
		}

		WHEN("crpGenerationContextWrite is called with format arguments")
		{
			crpGenerationContextWrite(cCtx, "Value: %d, String: %s", 42, "test");

			THEN("The formatted string is written")
			{
				CHECK(stream.str() == "Value: 42, String: test\n");
			}
		}

		WHEN("crpGenerationContextNewLine is called")
		{
			crpGenerationContextNewLine(cCtx);

			THEN("A newline is added")
			{
				CHECK(stream.str() == "\n");
			}
		}

		WHEN("crpGenerationContextEnterScope is called")
		{
			crpGenerationContextEnterScope(cCtx);

			THEN("An opening brace is written and indent level increases")
			{
				CHECK(stream.str() == "{\n");
				CHECK(indentLevel == 1);
			}
		}

		WHEN("crpGenerationContextLeaveScope is called after EnterScope")
		{
			crpGenerationContextEnterScope(cCtx);
			stream.str("");
			crpGenerationContextLeaveScope(cCtx, nullptr);

			THEN("A closing brace is written and indent level decreases")
			{
				CHECK(stream.str() == "}\n");
				CHECK(indentLevel == 0);
			}
		}

		WHEN("crpGenerationContextLeaveScope is called with a suffix")
		{
			crpGenerationContextEnterScope(cCtx);
			stream.str("");
			crpGenerationContextLeaveScope(cCtx, ";");

			THEN("The closing brace has the suffix")
			{
				CHECK(stream.str() == "};\n");
			}
		}

		WHEN("Writing with indentation using C API")
		{
			crpGenerationContextEnterScope(cCtx);
			stream.str("");
			crpGenerationContextWrite(cCtx, "indented line");

			THEN("The line has proper indentation")
			{
				CHECK(stream.str() == "\tindented line\n");
			}
		}

		WHEN("Multiple nested scopes using C API")
		{
			crpGenerationContextEnterScope(cCtx);
			crpGenerationContextEnterScope(cCtx);
			stream.str("");
			crpGenerationContextWrite(cCtx, "double indented");

			THEN("The line has double indentation")
			{
				CHECK(stream.str() == "\t\tdouble indented\n");
				CHECK(indentLevel == 2);
			}
		}

		WHEN("Building a complete function with C API")
		{
			crpGenerationContextWrite(cCtx, "void MyFunction()");
			crpGenerationContextEnterScope(cCtx);
			crpGenerationContextWrite(cCtx, "int x = %d;", 42);
			crpGenerationContextWrite(cCtx, "printf(\"Hello\\n\");");
			crpGenerationContextLeaveScope(cCtx, nullptr);

			THEN("The complete function is generated with proper formatting")
			{
				std::string expected =
					"void MyFunction()\n"
					"{\n"
					"\tint x = 42;\n"
					"\tprintf(\"Hello\\n\");\n"
					"}\n";
				CHECK(stream.str() == expected);
			}
		}
	}

	GIVEN("NULL context pointer")
	{
		WHEN("C API functions are called with NULL")
		{
			THEN("They do not crash")
			{
				CHECK_NOTHROW(crpGenerationContextWrite(nullptr, "test"));
				CHECK_NOTHROW(crpGenerationContextNewLine(nullptr));
				CHECK_NOTHROW(crpGenerationContextEnterScope(nullptr));
				CHECK_NOTHROW(crpGenerationContextLeaveScope(nullptr, nullptr));
			}
		}
	}

	GIVEN("GenerationContext with null pointers using C API")
	{
		cct::GenerationContext ctx;
		ctx.outputStream = nullptr;
		ctx.indentLevel = nullptr;

		auto* cCtx = reinterpret_cast<CrpGenerationContext*>(&ctx);

		WHEN("C API functions are called")
		{
			THEN("They do not crash")
			{
				CHECK_NOTHROW(crpGenerationContextWrite(cCtx, "test"));
				CHECK_NOTHROW(crpGenerationContextNewLine(cCtx));
				CHECK_NOTHROW(crpGenerationContextEnterScope(cCtx));
				CHECK_NOTHROW(crpGenerationContextLeaveScope(cCtx, nullptr));
			}
		}
	}
}

SCENARIO("crpGenerationContextGetPrivateData")
{
	GIVEN("A GenerationContext with no private data")
	{
		cct::GenerationContext ctx;
		ctx.outputStream = nullptr;
		ctx.indentLevel = nullptr;

		auto* cCtx = reinterpret_cast<CrpGenerationContext*>(&ctx);

		WHEN("crpGenerationContextGetPrivateData is called")
		{
			void* result = crpGenerationContextGetPrivateData(cCtx);

			THEN("It returns nullptr")
			{
				CHECK(result == nullptr);
			}
		}
	}

	GIVEN("A GenerationContext with private data set")
	{
		cct::GenerationContext ctx;
		ctx.outputStream = nullptr;
		ctx.indentLevel = nullptr;

		int dummyData = 42;
		ctx.pluginPrivateData = &dummyData;

		auto* cCtx = reinterpret_cast<CrpGenerationContext*>(&ctx);

		WHEN("crpGenerationContextGetPrivateData is called")
		{
			void* result = crpGenerationContextGetPrivateData(cCtx);

			THEN("It returns the correct pointer")
			{
				CHECK(result == &dummyData);
				CHECK(*static_cast<int*>(result) == 42);
			}
		}
	}

	GIVEN("A NULL context")
	{
		WHEN("crpGenerationContextGetPrivateData is called with NULL")
		{
			void* result = crpGenerationContextGetPrivateData(nullptr);

			THEN("It returns nullptr")
			{
				CHECK(result == nullptr);
			}
		}
	}
}

SCENARIO("FlattenAttributes")
{
	GIVEN("Scalar, floating and nested table attributes")
	{
		const TomlAttributes attributes = toml::parse_str("Max = 100\nId = \"sample.value\"\nStep = 0.1234567891\nRange = { Lo = 1, Hi = 2.5 }\nEnabled = true");
		const cct::FlatAttributes expected = {
			{"Enabled", "true"},
			{"Id", "sample.value"},
			{"Max", "100"},
			{"Range.Hi", "2.5"},
			{"Range.Lo", "1"},
			{"Step", "0.1234567891"},
		};

		WHEN("They are flattened")
		{
			const cct::FlatAttributes flat = cct::FlattenAttributes(attributes);

			THEN("Nested tables become dotted keys, sorted by key, with full precision")
			{
				CHECK(flat == expected);
			}
		}

		WHEN("They are flattened from several threads")
		{
			const TomlAttributes other = toml::parse_str("Id = \"other\"\nMin = -5");
			const cct::FlatAttributes otherExpected = {{"Id", "other"}, {"Min", "-5"}};

			std::atomic<int> mismatches = 0;
			std::vector<std::thread> threads;
			for (int t = 0; t < 8; ++t)
			{
				threads.emplace_back([&, t]()
									 {
					const bool useOther = (t % 2) != 0;
					for (int i = 0; i < 200; ++i)
					{
						if (cct::FlattenAttributes(useOther ? other : attributes) != (useOther ? otherExpected : expected))
							++mismatches;
					} });
			}
			for (std::thread& thread : threads)
				thread.join();

			THEN("Each call returns its own snapshot")
			{
				CHECK(mismatches == 0);
			}
		}
	}

	GIVEN("No attributes")
	{
		THEN("Nothing is flattened")
		{
			CHECK(cct::FlattenAttributes(TomlAttributes{}).empty());
			CHECK(cct::FlattenAttributes(toml::parse_str("")).empty());
		}
	}
}

SCENARIO("crpClassMemberForEachFlatAttribute with a NULL member")
{
	THEN("Nothing is visited")
	{
		int calls = 0;
		const std::size_t count = crpClassMemberForEachFlatAttribute(
			nullptr, [](const char*, const char*, void* userData)
			{ ++*static_cast<int*>(userData); }, &calls);
		CHECK(count == 0);
		CHECK(calls == 0);
	}
}
