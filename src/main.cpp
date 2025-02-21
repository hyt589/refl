#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>

#include "mustache.hpp"

constexpr auto reflection_name = "reflect";
constexpr auto bind_name = "reflection";

struct TemplateParamData {
    bool is_type = true;
    std::string name;
    std::string non_type_type_name;
    std::string non_type_value;
    std::string instantiated_type_name;
};
struct TypeData {
    const clang::Type* type_ptr = nullptr;
    std::string name;
    std::string qualified_name;
    std::vector<std::string> namespaces;
    bool is_templated = false;
    std::vector<TemplateParamData> template_params;
};
std::map<const clang::Type*, TypeData> g_type_infos; // needed for forward declarations

struct FieldData {
    std::string name;
    std::string qualified_name;
    std::string qualified_type_name;
    const clang::Type* type_ptr = nullptr;
};
struct MethodData {
    std::string name;
    std::string qualified_name;
    std::string qualified_return_type_name;
    std::string method_qualified_type_name;
    std::vector<std::string> param_type_list;
    const clang::Type* return_type_ptr = nullptr;
    std::vector<const clang::Type*> param_type_ptrs;
};
struct ReflectionData {
    std::string qualified_type_name;
    std::vector<FieldData> fields;
    std::vector<MethodData> methods;
    const clang::Type* type_ptr;
};

std::map<const clang::Type*, ReflectionData> g_reflection_table;

using MatchFinder = clang::ast_matchers::MatchFinder;
using MatchCallback = clang::ast_matchers::MatchFinder::MatchCallback;
using MatchResult = clang::ast_matchers::MatchFinder::MatchResult;

static inline clang::PrintingPolicy& ThePolicy() {
    static clang::PrintingPolicy policy(clang::LangOptions{});
    return policy;
}

template<typename T, typename = void>
struct is_class_decl : std::false_type {};
template<typename T>
struct is_class_decl<T, std::void_t<decltype(std::declval<T>().fields()), decltype(std::declval<T>().methods())>>
    : std::true_type {};

auto InspectType(clang::QualType qual_type, const clang::ASTContext& ast_ctx) {
    auto type_ptr = qual_type.getTypePtrOrNull();
    if (g_type_infos.find(type_ptr) != g_type_infos.end()) {
        return type_ptr;
    }
    TypeData result;
    result.type_ptr = type_ptr;
    auto policy = ast_ctx.getPrintingPolicy();
    // policy.SuppressScope = 1;
    result.qualified_name = qual_type.getAsString(policy);
    if (!qual_type->getAs<clang::RecordType>()) {
        return type_ptr;
    }
    auto decl = qual_type->getAs<clang::RecordType>()->getDecl();
    if (auto canonical_record_type = qual_type.getCanonicalType()->getAs<clang::RecordType>()) {
        decl = canonical_record_type->getDecl();
    }
    if (decl) {
        if (auto class_templ_spec_decl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
            result.is_templated = true;
            auto templ_decl = class_templ_spec_decl->getSpecializedTemplate()->getTemplatedDecl();
            auto param_list = templ_decl->getDescribedTemplateParams();

            int i = 0;
            for (const auto& arg : class_templ_spec_decl->getTemplateArgs().asArray()) {
                TemplateParamData template_param;
                template_param.name = "arg" + std::to_string(i);
                // arg.dump();
                if (arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
                    template_param.is_type = true;
                    template_param.instantiated_type_name = arg.getAsType().getAsString(ast_ctx.getPrintingPolicy());
                    InspectType(arg.getAsType(), ast_ctx);
                } else {
                    template_param.is_type = false;
                    template_param.non_type_type_name =
                        arg.getNonTypeTemplateArgumentType().getAsString(ast_ctx.getPrintingPolicy());
                    InspectType(arg.getNonTypeTemplateArgumentType(), ast_ctx);
                    // TODO: handle non integral kind
                    if (arg.getKind() == clang::TemplateArgument::Integral) {
                        template_param.non_type_value = std::to_string(arg.getAsIntegral().getExtValue());
                    }
                }
                i++;
                result.template_params.emplace_back(template_param);
            }
        }
        result.name = decl->getNameAsString();
        auto decl_ctx = decl->getDeclContext();
        while (decl_ctx) {
            if (decl_ctx->isNamespace()) {
                auto namespace_decl = clang::dyn_cast<clang::NamespaceDecl>(decl_ctx);
                result.namespaces.insert(result.namespaces.begin(), namespace_decl->getQualifiedNameAsString());
            }
            decl_ctx = decl_ctx->getParent();
        }
    }
    g_type_infos.insert({type_ptr, result});
    return type_ptr;
}

template<typename T>
std::enable_if_t<is_class_decl<T>::value, void>
ReflectImpl(const T* class_decl, const clang::ASTContext& ctx, ReflectionData& reflection) {
    for (const clang::FieldDecl* field : class_decl->fields()) {
        FieldData field_data;
        field_data.name = field->getNameAsString();
        field_data.qualified_name = field->getQualifiedNameAsString();
        field_data.qualified_type_name = field->getType().getAsString(ctx.getPrintingPolicy());
        InspectType(field->getType(), ctx);
        reflection.fields.emplace_back(field_data);
    }
    namespace mstch = kainjow::mustache;
    for (const clang::CXXMethodDecl* method : class_decl->methods()) {
        if (method->getKind() == clang::CXXMethodDecl::Kind::CXXConstructor) {
            continue;
        }
        if (method->getKind() == clang::CXXMethodDecl::Kind::CXXDestructor) {
            continue;
        }
        MethodData method_data;
        method_data.name = method->getNameAsString();
        method_data.qualified_name = method->getQualifiedNameAsString();
        method_data.qualified_return_type_name = method->getReturnType().getAsString(ctx.getPrintingPolicy());
        InspectType(method->getReturnType(), ctx);
        mstch::mustache method_qualified_type_template{
            R"({{return_type}} ({{class_name}}::*)({{#params}}{{{param_type_name}}}{{delimiter}}{{/params}}))"};
        mstch::data method_qualified_type_data{mstch::data::type::object};
        method_qualified_type_data.set("return_type", method_data.qualified_return_type_name);
        method_qualified_type_data.set("class_name", "T");

        mstch::data params{mstch::data::type::list};
        for (size_t i = 0; i < method->param_size(); i++) {
            auto param = method->getParamDecl(i);
            auto param_type = param->getType().getAsString(ctx.getPrintingPolicy());

            mstch::data param_data{mstch::data::type::object};
            method_data.param_type_list.emplace_back(param_type);
            param_data.set("param_type_name", param_type);
            param_data.set("delimiter", i == method->param_size() - 1 ? "" : ", ");
            params << param_data;
            InspectType(param->getType(), ctx);
        }
        method_qualified_type_data.set("params", params);
        method_data.method_qualified_type_name = method_qualified_type_template.render(method_qualified_type_data);
        reflection.methods.emplace_back(method_data);
    }
}

std::string GenerateClassReflectionCode(const ReflectionData& reflection) {
    namespace mstch = kainjow::mustache;

    mstch::mustache reflection_template{R"(
template <typename T> struct reflect<T, typename std::enable_if<std::is_same<T, {{{type_name}}}>::value, void >::type> {
    static constexpr const char * type_name() { return "{{{type_name}}}"; }
    static constexpr auto fields() {
        return std::make_tuple(
            {{#field_member_infos}}
                member_info<T, decltype(T::{{field_name}})>{ "{{field_name}}", &T::{{field_name}} } {{{delimiter}}}
            {{/field_member_infos}}
        );
    }
    static constexpr auto methods() {
        return std::make_tuple(
            {{#method_member_infos}}
                method_info<T, decltype(&T::{{method_name}})>{ "{{method_name}}", &T::{{method_name}} } {{{delimiter}}}
            {{/method_member_infos}}
        );
    }
};
    )"};

    mstch::data reflection_data{mstch::data::type::object};
    reflection_data.set("type_name", reflection.qualified_type_name);

    mstch::data field_member_checks{mstch::data::type::list};
    mstch::data method_member_checks{mstch::data::type::list};
    mstch::data field_member_infos{mstch::data::type::list};
    mstch::data method_member_infos{mstch::data::type::list};

    for (size_t i = 0; i < reflection.fields.size(); i++) {
        mstch::data member_check{mstch::data::type::object};
        member_check.set("field_name", reflection.fields[i].name);
        member_check.set("field_qualified_type_name", reflection.fields[i].qualified_type_name);
        field_member_checks << member_check;

        mstch::data member_info{mstch::data::type::object};
        member_info.set("field_name", reflection.fields[i].name);
        member_info.set("field_qualified_name", reflection.fields[i].qualified_name);
        member_info.set("delimiter", i == reflection.fields.size() - 1 ? "" : ",");
        field_member_infos << member_info;
    }

    for (size_t i = 0; i < reflection.methods.size(); i++) {
        mstch::data member_check{mstch::data::type::object};
        member_check.set("method_name", reflection.methods[i].name);
        member_check.set("method_return_type", reflection.methods[i].qualified_return_type_name);
        member_check.set("delimiter", i == reflection.methods.size() - 1 ? "," : "&&");

        mstch::data params{mstch::data::type::list};
        for (size_t j = 0; j < reflection.methods[i].param_type_list.size(); j++) {
            mstch::data param{mstch::data::type::object};
            param.set("param_type", reflection.methods[i].param_type_list[j]);
            param.set("delimiter", j == reflection.methods[i].param_type_list.size() - 1 ? "" : ", ");
            params << param;
        }
        member_check.set("params", params);
        method_member_checks << member_check;

        mstch::data member_info{mstch::data::type::object};
        member_info.set("method_name", reflection.methods[i].name);
        member_info.set("method_qualified_name", reflection.methods[i].qualified_name);
        member_info.set("delimiter", i == reflection.methods.size() - 1 ? "" : ",");
        method_member_infos << member_info;
    }

    reflection_data.set("field_member_checks", field_member_checks);
    reflection_data.set("field_member_infos", field_member_infos);
    reflection_data.set("method_member_checks", method_member_checks);
    reflection_data.set("method_member_infos", method_member_infos);
    auto result = reflection_template.render(reflection_data);
    return result;
}

std::string GenerateForwardDeclarationForType(const TypeData& type_data) {
    namespace mstch = kainjow::mustache;
    mstch::mustache forward_declaration_template{R"(
{{#openings}}
namespace {{namespace}} {
{{/openings}}
{{#template_decl}}
template <{{#template_params}}{{typename_or_type}} {{{param_name}}} {{delimiter}}{{/template_params}}> struct {{type_name}};
{{/template_decl}}
{{^template_decl}}
struct {{{type_name}}};
{{/template_decl}}
{{#closings}}
}
{{/closings}}
    )"};
    mstch::data forward_declaration_data{mstch::data::type::object};
    mstch::data openings{mstch::data::type::list};
    mstch::data closings{mstch::data::type::list};
    for (const auto& namespace_name : type_data.namespaces) {
        openings << mstch::data("namespace", namespace_name);
        closings << mstch::data();
    }
    forward_declaration_data.set("openings", openings);
    forward_declaration_data.set("closings", closings);
    if (type_data.is_templated) {
        std::stringstream ss;
        ss << type_data.name << "<";
        mstch::data template_decl{mstch::data::type::list};
        mstch::data template_info{mstch::data::type::object};
        mstch::data template_params{mstch::data::type::list};
        for (size_t i = 0; i < type_data.template_params.size(); i++) {
            const auto& p = type_data.template_params[i];
            mstch::data template_param{mstch::data::type::object};
            template_param.set("typename_or_type", p.is_type ? "typename" : p.non_type_type_name);
            template_param.set("param_name", p.name);
            std::string delimiter = i == type_data.template_params.size() - 1 ? "" : ",";
            template_param.set("delimiter", delimiter);
            template_params << template_param;

            ss << (p.is_type ? ("::" + p.instantiated_type_name) : p.non_type_value) << delimiter;
        }
        ss << ">";
        template_info.set("template_params", template_params);
        template_info.set("type_name", type_data.name);
        template_decl << template_info;
        forward_declaration_data.set("template_decl", template_decl);
        forward_declaration_data.set("type_name", ss.str());
    } else {
        forward_declaration_data.set("type_name", type_data.name);
    }
    return forward_declaration_template.render(forward_declaration_data);
}

auto Reflect(const clang::RecordDecl* record_decl) {
    auto& ctx = record_decl->getASTContext();
    ReflectionData reflection;
    if (auto specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record_decl)) {
        // reflection.qualified_type_name = specialization->getQualifiedNameAsString();
        ReflectImpl(specialization, ctx, reflection);
    } else if (auto decl = llvm::dyn_cast<clang::CXXRecordDecl>(record_decl)) {
        // reflection.qualified_type_name = decl->getQualifiedNameAsString();
        ReflectImpl(decl, ctx, reflection);
    }
    return reflection;
}

class ReflectHandler : public MatchCallback {
public:
    virtual void run(const MatchResult& result) override {
        auto node = result.Nodes.getNodeAs<clang::ClassTemplateSpecializationDecl>(bind_name);
        if (!node) {
            return;
        }
        auto specialization = node->getSpecializedTemplate();
        if (!specialization) {
            return;
        }
        auto templ_decl = specialization->getTemplatedDecl();
        if (!templ_decl->getDeclContext()->isTranslationUnit()) {
            return;
        }
        for (const auto& arg : node->getTemplateArgs().asArray()) {
            if (arg.getKind() != clang::TemplateArgument::ArgKind::Type) {
                continue;
            }
            auto qual_type = arg.getAsType();
            if (!qual_type->getAs<clang::RecordType>()) {
                continue;
            }
            InspectType(qual_type, node->getASTContext());
            auto type_ptr = qual_type.getTypePtrOrNull();
            if (g_reflection_table.find(type_ptr) != g_reflection_table.end()) {
                continue; // already reflected
            }
            auto record_decl = qual_type->getAs<clang::RecordType>()->getDecl();
            auto policy = node->getASTContext().getPrintingPolicy();
            policy.SuppressDefaultTemplateArgs = false;
            ReflectionData reflection_data = Reflect(record_decl);
            reflection_data.qualified_type_name = qual_type.getAsString(policy);
            reflection_data.type_ptr = type_ptr;
            g_reflection_table.insert({type_ptr, reflection_data});
            continue; // only first argument is important
        };
    }

private:
};

static llvm::cl::OptionCategory ReflectToolCategory("Reflect Tool Options");

static inline std::string GeneratePreambleCode() {
    namespace mstch = kainjow::mustache;
    mstch::mustache preamble_template{R"(
#pragma once
// preamble-begin
#include <type_traits>
#include <string>

template<class T, class V = void>
struct member_info {
    using value_type = V;
    std::string name;
    V T::* ptr = nullptr;
};

template<typename T, typename F>
struct method_info {
    using method_type = F;
    std::string_view name;
    F func_ptr;
};

template<typename T, typename Enable = void>
struct reflect {
private:
    static constexpr auto size = sizeof(T); // always trigger instantiation if T is templated
};

{{#forward_declarations}}
{{{forward_declaration}}}
{{/forward_declarations}}
// preamble-end
)"};
    mstch::data preamble_data{mstch::data::type::object};
    mstch::data forward_declarations{mstch::data::type::list};
    for (const auto& [type_ptr, type_data] : g_type_infos) {
        if (type_ptr->isBuiltinType()) {
            continue;
        }
        forward_declarations << mstch::data("forward_declaration", GenerateForwardDeclarationForType(type_data));
    }
    preamble_data.set("forward_declarations", forward_declarations);
    return preamble_template.render(preamble_data);
}

static inline std::string GenerateReflectionCode() {
    namespace mstch = kainjow::mustache;
    mstch::mustache reflection_code_template{R"(
// reflection-begin
{{#reflections}}
{{{class_reflection}}}
{{/reflections}}
// reflection-end
    )"};
    mstch::data reflection_code_data{mstch::data::type::object};
    mstch::data reflections{mstch::data::type::list};
    for (const auto& [type_ptr, reflection] : g_reflection_table) {
        reflections << mstch::data("class_reflection", GenerateClassReflectionCode(reflection));
    }
    reflection_code_data.set("reflections", reflections);
    return reflection_code_template.render(reflection_code_data);
}

static inline std::string GenerateFullReflectionFile() {
    namespace mstch = kainjow::mustache;
    mstch::mustache code_template{R"(
{{{preamble}}}

{{{reflections}}}
    )"};
    mstch::data code_data{mstch::data::type::object};
    code_data.set("preamble", GeneratePreambleCode());
    code_data.set("reflections", GenerateReflectionCode());
    return code_template.render(code_data);
};

int main(int argc, const char** argv) {
    using namespace clang;
    using namespace clang::ast_matchers;
    using namespace clang::tooling;

    auto OptionsParser = CommonOptionsParser::create(argc, argv, ReflectToolCategory);
    ClangTool Tool(OptionsParser->getCompilations(), OptionsParser->getSourcePathList());

    MatchFinder Finder;
    ReflectHandler Handler;

    // Match all uses of reflect<T> in type locations
    Finder.addMatcher(
        classTemplateSpecializationDecl(hasName(reflection_name), isTemplateInstantiation()).bind(bind_name), &Handler);

    Tool.run(newFrontendActionFactory(&Finder).get());

    std::cout << GenerateFullReflectionFile() << std::endl;

    return 0;
}
