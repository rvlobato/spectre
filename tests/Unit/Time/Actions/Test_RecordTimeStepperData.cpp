// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "tests/Unit/TestingFramework.hpp"

#include <string>
#include <utility>
// IWYU pragma: no_include <unordered_map>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataBox/DataBoxTag.hpp"
#include "DataStructures/DataBox/Prefixes.hpp"
#include "Time/Actions/RecordTimeStepperData.hpp"  // IWYU pragma: keep
// IWYU pragma: no_include "Time/History.hpp"
#include "Time/Slab.hpp"
#include "Time/Tags.hpp"
#include "Time/Time.hpp"
#include "Time/TimeId.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"
#include "tests/Unit/ActionTesting.hpp"

namespace {
struct Var : db::SimpleTag {
  static std::string name() noexcept { return "Var"; }
  using type = double;
};

struct System {
  using variables_tag = Var;
};

using variables_tag = Var;
using dt_variables_tag = Tags::dt<Var>;
using history_tag =
    Tags::HistoryEvolvedVariables<variables_tag, dt_variables_tag>;

struct Metavariables;
struct component : ActionTesting::MockArrayComponent<
                       Metavariables, int, tmpl::list<>,
                       tmpl::list<Actions::RecordTimeStepperData>> {
  using simple_tags = db::AddSimpleTags<Tags::TimeId, variables_tag,
                                        dt_variables_tag, history_tag>;
  using compute_tags = db::AddComputeTags<Tags::Time>;
  using initial_databox =
      db::compute_databox_type<tmpl::append<simple_tags, compute_tags>>;
};

struct Metavariables {
  using system = System;
  using component_list = tmpl::list<component>;
  using const_global_cache_tag_list = tmpl::list<>;
};
}  // namespace

SPECTRE_TEST_CASE("Unit.Time.Actions.RecordTimeStepperData",
                  "[Unit][Time][Actions]") {

  const Slab slab(1., 3.);
  const TimeId time_id(true, 8, slab.start());

  history_tag::type history{};
  history.insert(slab.end(), 2., 3.);

  using MockRuntimeSystem = ActionTesting::MockRuntimeSystem<Metavariables>;
  using LocalAlgsTag = MockRuntimeSystem::LocalAlgorithmsTag<component>;
  MockRuntimeSystem::TupleOfMockDistributedObjects local_algs{};
  tuples::get<LocalAlgsTag>(local_algs)
      .emplace(0, ActionTesting::MockDistributedObject<component>{
                      db::create<typename component::simple_tags,
                                 typename component::compute_tags>(
                          time_id, 4., 5., std::move(history))});
  MockRuntimeSystem runner{{}, std::move(local_algs)};

  runner.next_action<component>(0);
  const auto& box = runner.algorithms<component>()
                        .at(0)
                        .get_databox<typename component::initial_databox>();

  const auto& new_history = db::get<history_tag>(box);
  CHECK(new_history.size() == 2);
  CHECK(*new_history.begin() == slab.end());
  CHECK(new_history.begin().value() == 2.);
  CHECK(new_history.begin().derivative() == 3.);
  CHECK(*(new_history.begin() + 1) == slab.start());
  CHECK((new_history.begin() + 1).value() == 4.);
  CHECK((new_history.begin() + 1).derivative() == 5.);
}
