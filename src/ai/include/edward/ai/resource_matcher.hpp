#pragma once
#include <QString>
#include <QVector>
namespace edward::ai {
struct TargetIntent { QString target; QString runtime; QString scene; QString action; };
struct VerifiedResource { QString id; QString target; QString runtime; QString scene; QString action; bool entitled = true; };
struct ResourceCandidate { QString id; double score = 0; };
struct MatchDecision { bool requiresClarification = true; QString resourceId; QVector<ResourceCandidate> candidates; };
class ResourceMatcher final { public: QVector<ResourceCandidate> rank(const TargetIntent&, const QVector<VerifiedResource>&) const; MatchDecision choose(const QVector<ResourceCandidate>&) const; };
}  // namespace edward::ai
