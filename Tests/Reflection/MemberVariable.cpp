//
// Created by arthur on 04/07/2025.
//
#define CATCH_CONFIG_RUNNER
#include <ConcertoReflectionPackage.gen.hpp>
#include <ConcertoReflectionTestsPackage.gen.hpp>

#include <Concerto/Reflection/GlobalNamespace/GlobalNamespace.hpp>
#include <Concerto/Reflection/PackageLoader/PackageLoader.hpp>

#include "SampleBar.refl.hpp"
#include <catch2/catch_test_macros.hpp>

SCENARIO("MemberVariable")
{
	using namespace std::string_view_literals;
	GIVEN("The core package")
	{
		WHEN("The package is initialized")
		{
			cct::refl::PackageLoader packageLoader;
			REQUIRE(packageLoader.AddPackage(CreateConcertoReflectionPackage()));
			REQUIRE(packageLoader.AddPackage(CreateConcertoReflectionTestsPackage()));
			packageLoader.LoadPackages();

			THEN("We are getting the member variable of 'SampleBar'")
			{
				const cct::refl::Class* sampleBarClass = cct::sample::SampleBar::GetClass();
				REQUIRE(sampleBarClass);

				const size_t memberVariableCount = sampleBarClass->GetMemberVariableCount();
				REQUIRE(memberVariableCount > 0);
				CHECK(memberVariableCount == 1);

				auto sampleBarInstance = sampleBarClass->CreateDefaultObject();
				REQUIRE(sampleBarInstance);

				const cct::refl::Object* sampleBar1 = cct::sample::SampleBar::GetClass()->GetMemberVariable("bar", *sampleBarInstance);
				REQUIRE(sampleBar1);
				CHECK(sampleBar1->GetDynamicClass() == cct::refl::Int32::GetClass());

				const cct::refl::Object* sampleBar2 = sampleBarInstance->GetDynamicClass()->GetMemberVariable(0, *sampleBarInstance);
				CHECK(sampleBar1 == sampleBar2);

				sampleBar2 = sampleBarInstance->GetMemberVariable("bar");
				CHECK(sampleBar1 == sampleBar2);

				sampleBar2 = sampleBarInstance->GetMemberVariable(0);
				CHECK(sampleBar1 == sampleBar2);

				const cct::refl::MemberVariable* sampleBarMemberVariable1 = cct::sample::SampleBar::GetClass()->GetMemberVariable("bar");
				REQUIRE(sampleBarMemberVariable1);
				CHECK(sampleBarMemberVariable1->GetName() == "bar"sv);
				CHECK(sampleBarMemberVariable1->GetIndex() == 0);
				CHECK(sampleBarMemberVariable1->GetType() == cct::refl::Int32::GetClass());

				const cct::refl::MemberVariable* sampleBarMemberVariable2 = cct::sample::SampleBar::GetClass()->GetMemberVariable(0);
				CHECK(sampleBarMemberVariable1 == sampleBarMemberVariable2);
			}

			THEN("We are getting the native member variable of 'SampleBar'")
			{
				const cct::refl::Class* sampleBarClass = cct::sample::SampleBar::GetClass();
				REQUIRE(sampleBarClass);
				CHECK(sampleBarClass->GetNativeMemberVariableCount() == 1);

				auto sampleBarInstance = sampleBarClass->CreateDefaultObject();
				REQUIRE(sampleBarInstance);

				// Global native-member indices are offset by the base class's own native
				// members (e.g. cct::refl::Object::m_uuid) — SampleBar::m_nativeType is
				// declared first locally, but not necessarily at global index 0.
				const cct::refl::Class* baseClass = sampleBarClass->GetBaseClass();
				REQUIRE(baseClass);
				const std::size_t baseNativeCount = baseClass->GetTotalNativeMemberCount();

				const cct::refl::NativeMemberVariable* nativeMemberVariable = sampleBarClass->GetNativeMemberVariable(baseNativeCount);
				REQUIRE(nativeMemberVariable);
				CHECK(nativeMemberVariable->GetName() == "nativeType"sv);
				CHECK(nativeMemberVariable->GetIndex() == baseNativeCount);
				CHECK(nativeMemberVariable->GetTypeId() == cct::TypeId<cct::Int32>());

				const cct::refl::NativeMemberVariable* nativeMemberVariable1 = sampleBarClass->GetNativeMemberVariable("nativeType"sv);
				REQUIRE(nativeMemberVariable1);
				CHECK(nativeMemberVariable1->GetName() == "nativeType"sv);
				CHECK(nativeMemberVariable1->GetIndex() == baseNativeCount);
				CHECK(nativeMemberVariable1->GetTypeId() == cct::TypeId<cct::Int32>());

				const cct::Int32* memberVariable1 = sampleBarInstance->GetNativeMemberVariable<cct::Int32>(baseNativeCount);
				REQUIRE(memberVariable1);
				CHECK(*memberVariable1 == 42);

				const cct::Int32* memberVariable2 = sampleBarInstance->GetNativeMemberVariable<cct::Int32>("nativeType");
				CHECK(memberVariable2);
				CHECK(memberVariable2 == memberVariable1);
			}
		}
	}
}

SCENARIO("MemberVariable attributes")
{
	using namespace std::string_view_literals;
	GIVEN("The core and test packages are loaded")
	{
		cct::refl::PackageLoader packageLoader;
		REQUIRE(packageLoader.AddPackage(CreateConcertoReflectionPackage()));
		REQUIRE(packageLoader.AddPackage(CreateConcertoReflectionTestsPackage()));
		packageLoader.LoadPackages();

		THEN("A non Min/Max attribute of 'SampleBar::m_bar' is propagated")
		{
			const cct::refl::MemberVariable* bar = cct::sample::SampleBar::GetClass()->GetMemberVariable("bar"sv);
			REQUIRE(bar);
			CHECK(bar->HasAttribute("Test"sv));
			CHECK(bar->GetAttribute("Test"sv) == "test"sv);
		}

		THEN("Every attribute of a reflected member is propagated, including Id")
		{
			const cct::refl::Class* klass = cct::sample::MemberAttributeSample::GetClass();
			REQUIRE(klass);
			const cct::refl::MemberVariable* value = klass->GetMemberVariable("value"sv);
			REQUIRE(value);

			CHECK(value->GetAttribute("Id"sv) == "sample.value"sv);
			CHECK(value->GetAttribute("Min"sv) == "0"sv);
			CHECK(value->GetAttribute("Max"sv) == "100"sv);
			// Full precision: "%g" would have produced "0.123457".
			CHECK(value->GetAttribute("Step"sv) == "0.1234567891"sv);
			// Nested tables are flattened into dotted keys.
			CHECK(value->GetAttribute("Range.Lo"sv) == "1"sv);
			CHECK(value->GetAttribute("Range.Hi"sv) == "2.5"sv);
			CHECK(value->GetAttributes().size() == 9);
		}

		THEN("Keys and values needing escaping in generated C++ round-trip intact")
		{
			const cct::refl::MemberVariable* value = cct::sample::MemberAttributeSample::GetClass()->GetMemberVariable("value"sv);
			REQUIRE(value);

			CHECK(value->GetAttribute("Label"sv) == "quote \" backslash \\ tab \t newline \n braces {} percent %s"sv);
			CHECK(value->GetAttribute("Unicode"sv) == "caf\xC3\xA9"sv);
			CHECK(value->GetAttribute("quoted \"key\""sv) == "true"sv);
		}

		THEN("Every attribute of a native member is propagated")
		{
			const cct::refl::NativeMemberVariable* native = cct::sample::MemberAttributeSample::GetClass()->GetNativeMemberVariable("native"sv);
			REQUIRE(native);
			CHECK(native->GetAttribute("Id"sv) == "sample.native"sv);
			CHECK(native->GetAttribute("Min"sv) == "-5"sv);
			CHECK(native->GetAttributes().size() == 2);
		}
	}
}

SCENARIO("Typed Object::GetMemberVariable<T> on a derived class")
{
	using namespace std::string_view_literals;
	GIVEN("A default 'SampleBar' seen through its cct::refl::Object base")
	{
		cct::refl::PackageLoader packageLoader;
		REQUIRE(packageLoader.AddPackage(CreateConcertoReflectionPackage()));
		REQUIRE(packageLoader.AddPackage(CreateConcertoReflectionTestsPackage()));
		packageLoader.LoadPackages();

		std::unique_ptr<cct::refl::Object> instance = cct::sample::SampleBar::GetClass()->CreateDefaultObject();
		REQUIRE(instance);
		cct::refl::Object& object = *instance;
		const cct::refl::Object& constObject = *instance;

		const cct::refl::Object* untyped = object.GetMemberVariable("bar"sv);
		REQUIRE(untyped);

		THEN("The typed lookups resolve through the dynamic class")
		{
			cct::refl::Int32* byName = object.GetMemberVariable<cct::refl::Int32>("bar"sv);
			const cct::refl::Int32* byNameConst = constObject.GetMemberVariable<cct::refl::Int32>("bar"sv);
			cct::refl::Int32* byIndex = object.GetMemberVariable<cct::refl::Int32>(std::size_t{0});
			const cct::refl::Int32* byIndexConst = constObject.GetMemberVariable<cct::refl::Int32>(std::size_t{0});

			REQUIRE(byName);
			CHECK(byName == untyped);
			CHECK(byNameConst == untyped);
			CHECK(byIndex == untyped);
			CHECK(byIndexConst == untyped);
			CHECK(*byName == 42);
		}

		THEN("An unknown member name yields nullptr")
		{
			CHECK(object.GetMemberVariable<cct::refl::Int32>("missing"sv) == nullptr);
			CHECK(constObject.GetMemberVariable<cct::refl::Int32>("missing"sv) == nullptr);
		}
	}
}