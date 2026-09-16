#pragma once
#include "edward/ai/action_plan.hpp"
#include <QString>
namespace edward::ai { struct AiResult { enum class Kind { Conversation, Clarification, ActionPlan, Unsupported }; Kind kind = Kind::Conversation; QString text; std::optional<ActionPlan> plan; }; class AiOrchestrator final { public: AiResult handle(const QString& modelOutput, const ProjectSnapshot&) const; }; }
