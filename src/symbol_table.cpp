#include <cpy/symbol_table.hpp>
#include <stdexcept>


void SymbolTable::add_function (const FunctionDef& def, std::vector<ResolvedSymbol> params, VarType return_type)
{
  m_resolved_functions.emplace(def.name,
                               ResolvedFunction{
                                 .params = std::move(params),
                                 .return_type = return_type,
                                 .ast = &def
                               });
}

void SymbolTable::add_variable(const std::string_view name, const VarType type, const uint32_t position)
{
  if (m_scope_tracker.empty())
    throw std::runtime_error{"SymbolTable::add_variable(): scope tracker is empty"};

  m_scope_tracker.top()->vars.emplace(name, ResolvedSymbol{.name = name, .type = type, .position = position});
}


bool SymbolTable::has_visible_variable(const std::string_view name, const NodeBlock& block, const uint32_t position) const
{
  const auto resolved = get_resolved_variable(name, &block, true);
  return resolved != nullptr && resolved->position < position;
}

std::optional<BuiltInType> SymbolTable::get_type(const std::string_view type_name) const
{
  const auto it = BuiltInTypes.find(type_name) ;
  if (it == rg::end(BuiltInTypes))
   return std::nullopt;

  return it->second;
}

const ResolvedFunction& SymbolTable::get_function(const std::string_view name) const
{
  const auto& user_it = m_resolved_functions.find(name);

  if (user_it != m_resolved_functions.cend()) {
    return user_it->second;
  }
  else {
    return get_native_function(name).signature;
  }
}

const ResolvedSymbol& SymbolTable::get_variable(const std::string_view name, const NodeBlock& block) const
{
  if (const auto * symbol = get_resolved_variable(name, &block, true); symbol)
    return *symbol;

  throw std::runtime_error{"SymbolTable::get_variable() - var does not exist"};
}

std::optional<ResolvedVariableMap> SymbolTable::get_variables (const NodeBlock& block) const
{
  std::optional<ResolvedVariableMap> result;

  if (const auto it = m_scopes.find(&block); it != m_scopes.cend())
    result = it->second->vars;

  return result;
}

void SymbolTable::enter_scope(const NodeBlock * node)
{
  const auto parent = m_scope_tracker.empty() ? nullptr : m_scope_tracker.top();
  auto [it, emplaced] = m_scopes.emplace(node, std::make_unique<Scope>(parent));
  m_scope_tracker.push(it->second.get());
}

void SymbolTable::exit_scope()
{
  if (!m_scope_tracker.empty()) {
    m_scope_tracker.pop();
  }
}

void SymbolTable::dump (std::ostream& os) const
{
  os << "-- Functions --\n";
  for(const auto& [name, resolved] : m_resolved_functions)
  {
    os << name << '\n';
    os << "  - return: "<< to_string(resolved.return_type) << '\n' ;
    os << "  - params:\n";
    for(const auto& symbol : resolved.params) {
      os << std::setw(8) << symbol.name << " : " << (symbol.is_ref ? "ref " : "") << to_string(symbol.type) << '\n';
    }
  }

  os << "\n-- Variables --\n";
  for (const auto& [block, scope] : m_scopes)
  {
    std::cout << "Block: [" << block->source.start << ", " << block->source.end <<  "]\n";
    for (const auto& [var, resolved] : scope->vars) {
      std::cout << resolved.name << ":" << (resolved.is_ref ? "ref" : "") << to_string(resolved.type) << "\n";
    }
  }
}

const ResolvedSymbol * SymbolTable::get_resolved_variable(const std::string_view name, const NodeBlock * block, const bool visit_parents) const
{
  auto in_scope = [&name](const Scope * scope) -> const ResolvedSymbol*
  {
    const auto it = scope->vars.find(name);
    return it == scope->vars.cend() ? nullptr : &it->second;
  };

  const auto it_scopes = m_scopes.find(block) ;
  if (it_scopes == m_scopes.cend()) {
    return nullptr;
  }

  // check NodeBlock's scope
  const Scope * scope = it_scopes->second.get();
  const auto resolved = in_scope(scope);

  if (!visit_parents || resolved) {
    return resolved;
  }

  // travel up parents
  for (scope = scope->parent; scope ; scope = scope->parent)
  {
    if (const auto resolved = in_scope(scope); resolved)
      return resolved;
  }
  return nullptr;
}
