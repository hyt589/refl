#include <clang/AST/Type.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <iostream>

constexpr auto reflection_name = "reflect";
constexpr auto bind_name = "reflection";

struct FieldData {
  std::string name;
  std::string qualified_type_name;
};
struct MethodData {
  std::string method_name;
  std::vector<std::pair<std::string, std::string>>
      param_list; // each param = name, type_name
  std::string qualified_return_type_name;
  std::string method_qualified_type_name;
};
struct ReflectionData {
  std::string qualified_type_name;
  std::vector<FieldData> fields;
  std::vector<MethodData> methods;
};

using MatchFinder = clang::ast_matchers::MatchFinder;
using MatchCallback = clang::ast_matchers::MatchFinder::MatchCallback;
using MatchResult = clang::ast_matchers::MatchFinder::MatchResult;

static inline clang::PrintingPolicy& ThePolicy() {
  static clang::PrintingPolicy policy(clang::LangOptions{});
  return policy;
}

std::string
GetTypeName(clang::QualType qt,
            const clang::ClassTemplateSpecializationDecl *ctsd = nullptr) {
  ThePolicy().SuppressTagKeyword = true;
  if (qt->isLValueReferenceType()) {
    auto pointee_type = qt->getAs<clang::LValueReferenceType>()->getPointeeType();
    return GetTypeName(pointee_type, ctsd) + " &";
  }
  if (qt->isRValueReferenceType()) {
    auto pointee_type = qt->getAs<clang::RValueReferenceType>()->getPointeeType();
    return GetTypeName(pointee_type, ctsd) + " &&";
  }
  if (qt->isTemplateTypeParmType()) {
    if (!ctsd) {
      return qt.getAsString(ThePolicy());
    }
    auto ttpt = qt->getAs<clang::TemplateTypeParmType>();
    auto real_type =
        ctsd->getTemplateArgs().asArray()[ttpt->getIndex()].getAsType();
    return GetTypeName(real_type, ctsd);
  }
  return qt.getAsString(ThePolicy());
}

void Reflect(const clang::RecordDecl *record_decl) {
  // record_decl->dump();
  ReflectionData reflection;
  if (auto specialization =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record_decl)) {
    if (auto specialized_template = specialization->getSpecializedTemplate()) {
      auto decl = specialized_template->getTemplatedDecl();
      reflection.qualified_type_name = decl->getQualifiedNameAsString();
      std::cout << decl->getQualifiedNameAsString() << std::endl;
      for (auto field : decl->fields()) {
        FieldData field_data;
        field_data.name = field->getNameAsString();
        field_data.qualified_type_name =
            GetTypeName(field->getType(), specialization);
        std::cout << "field_data.name: " << field_data.name << std::endl;
        std::cout << "field_data.type_name: " << field_data.qualified_type_name
                  << std::endl;
      }
      for (auto method : decl->methods()) {
        MethodData method_data;
        method_data.method_name = method->getNameAsString();
        method_data.qualified_return_type_name =
            GetTypeName(method->getReturnType(), specialization);
        method_data.method_qualified_type_name = GetTypeName(method->getType(), specialization);
        std::cout << "method_data.method_name: " << method_data.method_name
                  << std::endl;
        std::cout << "method_data.return_type_name: "
                  << method_data.qualified_return_type_name << std::endl;
        std::cout << "method_data.method_qualified_type_name: " << method_data.method_qualified_type_name << std::endl;
        for (auto it = method->param_begin(); it != method->param_end();
             it++) {
          auto param = *it;
          std::cout << "param->getName().str(): " << param->getName().str() << std::endl;
          std::cout << "GetTypeName(param->getType()): " << GetTypeName(param->getType(), specialization) << std::endl;
        }
      }
    }
    for (const auto arg : specialization->getTemplateArgs().asArray()) {
      auto qual_type = arg.getAsType();
      if (auto qual_record = qual_type->getAs<clang::RecordType>()->getDecl()) {
        std::cout << qual_record->getQualifiedNameAsString() << std::endl;
      }
    }
  } else if (auto decl = llvm::dyn_cast<clang::CXXRecordDecl>(record_decl)) {
    decl->dump();
  }
}

class ReflectHandler : public MatchCallback {
public:
  virtual void run(const MatchResult &result) override {
    auto node = result.Nodes.getNodeAs<clang::ClassTemplateSpecializationDecl>(
        bind_name);
    if (!node) {
      std::cerr
          << "Matched node is not `clang::ClassTemplateSpecializationDecl`"
          << std::endl;
      return;
    }
    auto specialization = node->getSpecializedTemplate();
    if (!specialization) {
      std::cerr << "Matched node does not have specialized template"
                << std::endl;
      return;
    }
    auto templ_decl = specialization->getTemplatedDecl();
    if (!templ_decl) {
      std::cerr << "Matched node does not have a TemplateDecl" << std::endl;
      return;
    }
    if (!templ_decl->getDeclContext()->isTranslationUnit()) {
      std::cerr << "Matched template's DeclContext is not TranslationUnit"
                << std::endl;
      return;
    }
    for (const auto &arg : node->getTemplateArgs().asArray()) {
      // arg.dump();
      if (arg.getKind() != clang::TemplateArgument::ArgKind::Type) {
        std::cerr << "Wrong ArgKind: " << arg.getKind() << std::endl;
        continue;
      }
      auto qual_type = arg.getAsType();
      // qual_type.dump();
      if (!qual_type->getAs<clang::RecordType>()) {
        continue;
      }
      ReflectionData reflection_data;
      auto record_decl = qual_type->getAs<clang::RecordType>()->getDecl();
      Reflect(record_decl);
    };
  }

private:
};

static llvm::cl::OptionCategory ReflectToolCategory("Reflect Tool Options");

int main(int argc, const char **argv) {
  using namespace clang;
  using namespace clang::ast_matchers;
  using namespace clang::tooling;

  auto OptionsParser =
      CommonOptionsParser::create(argc, argv, ReflectToolCategory);
  clang::tooling::ClangTool Tool(OptionsParser->getCompilations(),
                                 OptionsParser->getSourcePathList());

  MatchFinder Finder;
  ReflectHandler Handler;

  // Match all uses of reflect<T> in type locations
  Finder.addMatcher(classTemplateSpecializationDecl(hasName(reflection_name),
                                                    isTemplateInstantiation())
                        .bind(bind_name),
                    &Handler);

  Tool.run(newFrontendActionFactory(&Finder).get());

  return 0;
}
