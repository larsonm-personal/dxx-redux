"""The station validator rejects empty fixtures and vacuous suppression passes."""

import copy
import json
import unittest

import run_public_matcen_stations as stations


def report(game):
    level, centers, energy = stations.PROFILES[game]
    cases = []
    for difficulty in (2, 4):
        for center, (station, segment) in enumerate(centers):
            for policy in range(4):
                births = [
                    dict(
                        round=round,
                        frame=index * 30 + 1,
                        object=80,
                        id=3,
                        signature=100 + round * 10 + index,
                        shields=65536,
                    )
                    for round in ((1, 2) if policy == 0 else (1,) if policy == 1 else ())
                    for index in range(2)
                ]
                cases.append(
                    dict(
                        difficulty=difficulty,
                        center=center,
                        policy=policy,
                        station=station,
                        segment=segment,
                        energy_centers=energy,
                        cleared_authored_robots=[],
                        first_births=2 if policy < 2 else 0,
                        second_births=2 if policy == 0 else 0,
                        first_lives=3 - int(policy != 2 and (game == "d1" or difficulty != 4 or policy == 1)),
                        second_lives=3
                        - int(policy != 2 and (game == "d1" or difficulty != 4 or policy == 1))
                        - int(policy == 0 and (game == "d1" or difficulty != 4)),
                        energy_state_preserved=True,
                        activation_count=(2, 1, 0, 1)[policy],
                        births=births,
                    )
                )
    dispatch_cases = []
    for scenario in ("wall_dispatch", "sub", "reverse", "no_dispatch", "empty_mask"):
        first, second = (0, len(centers) - 1) if scenario == "reverse" else (len(centers) - 1, 0)
        dispatched = scenario != "no_dispatch"
        dispatch_cases.append(
            dict(
                scenario=scenario,
                synthetic_links=True,
                zeroed_robot_masks=scenario == "empty_mask",
                authored_triggers=[dict(links=[[7, 2]])],
                source_segment=1,
                source_side=0,
                trigger=0,
                links=[centers[index][1] for index in (first, second, first)],
                mapping=[
                    dict(
                        ordinal=index, station=station, segment=segment, robot_flags=[8] + ([0] if game == "d2" else [])
                    )
                    for index, (station, segment) in enumerate(centers)
                ],
                cleared_authored_robots=[],
                activation_counts=[int(dispatched and index in (first, second)) for index in range(len(centers))],
                activation_lights=[
                    dict(signature=index + 20, segment=centers[center][1])
                    for index, center in enumerate((first, second))
                ]
                if dispatched
                else [],
                births=[
                    dict(
                        frame=frame,
                        object=90 + center,
                        signature=100 + frame * 10 + center,
                        id=3,
                        shields=65536,
                        ordinal=center,
                        station=centers[center][0],
                        segment=centers[center][1],
                    )
                    for frame in (1, 30)
                    for center in (0, len(centers) - 1)
                ]
                if scenario not in ("no_dispatch", "empty_mask")
                else [],
            )
        )
    return dict(
        dispatch_cases=dispatch_cases,
        schema=1,
        scope="controlled_public_station_loop",
        game=game,
        level=level,
        matcens=len(centers),
        passed=True,
        cases=cases,
    )


class MatcenEvidenceTests(unittest.TestCase):
    def test_both_real_nonempty_profiles(self):
        self.assertEqual(stations.validate_report(report("d1"), "d1"), 16)
        self.assertEqual(stations.validate_report(report("d2"), "d2"), 24)

    def test_dispatch_negative_controls(self):
        for game in ("d1", "d2"):
            for mutation in (
                "missing",
                "no_births",
                "mask",
                "segment",
                "ordinal",
                "station",
                "order",
                "duplicate",
                "no_dispatch",
                "empty_mask",
                "authored",
                "synthetic",
                "frame",
                "type",
                "sub_timing",
                "missing_origin",
            ):
                with self.subTest(game=game, mutation=mutation):
                    value = report(game)
                    row = value["dispatch_cases"][0]
                    birth = row["births"][0]
                    if mutation == "missing":
                        del value["dispatch_cases"]
                    elif mutation == "no_births":
                        row["births"] = []
                    elif mutation == "mask":
                        row["mapping"][0]["robot_flags"][0] = 0
                    elif mutation == "segment":
                        birth["segment"] += 1
                    elif mutation == "ordinal":
                        birth["ordinal"] = len(stations.PROFILES[game][1]) - 1
                    elif mutation == "station":
                        birth["station"] += 1
                    elif mutation == "order":
                        row["activation_lights"].reverse()
                    elif mutation == "duplicate":
                        row["activation_counts"][0] += 1
                    elif mutation in ("no_dispatch", "empty_mask"):
                        value["dispatch_cases"][3 if mutation == "no_dispatch" else 4]["births"] = copy.deepcopy(
                            row["births"]
                        )
                    elif mutation == "authored":
                        row["authored_triggers"] = []
                    elif mutation == "synthetic":
                        row["synthetic_links"] = False
                    elif mutation == "frame":
                        birth["frame"] = 2400
                    elif mutation == "type":
                        birth["id"] = 4
                    elif mutation == "sub_timing":
                        value["dispatch_cases"][1]["births"][0]["frame"] = 0
                    elif mutation == "missing_origin":
                        del birth["ordinal"]
                    with self.assertRaises(stations.EvidenceError):
                        stations.validate_report(value, game)

    def test_rejects_vacuous_or_incorrect_results(self):
        for game in ("d1", "d2"):
            for mutation in (
                "empty_level",
                "duplicate",
                "missing_case",
                "no_production",
                "one_robot",
                "paused_production",
                "one_round_reactivation",
                "changed_first_round",
                "no_energy_centers",
                "wrong_lives",
                "missing_identity",
                "boolean_counter",
                "hidden_cleared_robot",
                "reused_signature",
                "consumed_retrigger_life",
                "mutated_energy_center",
                "repeated_frame",
                "reordered_births",
            ):
                with self.subTest(game=game, mutation=mutation):
                    value = report(game)
                    case = value["cases"][0]
                    if mutation == "empty_level":
                        value["matcens"] = 0
                    elif mutation == "duplicate":
                        value["cases"][1] = copy.deepcopy(case)
                    elif mutation == "missing_case":
                        value["cases"].pop()
                    elif mutation == "no_production":
                        case["births"] = []
                        case["first_births"] = case["second_births"] = 0
                    elif mutation == "one_robot":
                        case["births"].pop(0)
                        case["first_births"] = 1
                    elif mutation == "paused_production":
                        value["cases"][2]["births"] = copy.deepcopy(case["births"])
                    elif mutation == "one_round_reactivation":
                        value["cases"][1]["births"] = copy.deepcopy(case["births"])
                        value["cases"][1]["second_births"] = 2
                    elif mutation == "changed_first_round":
                        value["cases"][1]["births"][0]["frame"] += 1
                    elif mutation == "no_energy_centers":
                        case["energy_centers"] = 0
                    elif mutation == "wrong_lives":
                        case["first_lives"] = 3
                    elif mutation == "missing_identity":
                        del case["births"][0]["signature"]
                    elif mutation == "boolean_counter":
                        case["center"] = False
                    elif mutation == "hidden_cleared_robot":
                        del case["cleared_authored_robots"]
                    elif mutation == "consumed_retrigger_life":
                        value["cases"][1]["second_lives"] -= 1
                    elif mutation == "mutated_energy_center":
                        case["energy_state_preserved"] = False
                    elif mutation == "reused_signature":
                        case["births"][1]["signature"] = case["births"][0]["signature"]
                    elif mutation == "repeated_frame":
                        case["births"][1]["frame"] = case["births"][0]["frame"]
                    elif mutation == "reordered_births":
                        case["births"][0], case["births"][1] = case["births"][1], case["births"][0]
                    with self.assertRaises(stations.EvidenceError):
                        stations.validate_report(value, game)


def historical_report(game):
    """Rehydrate only incidental identities for self-contained golden tests."""
    golden = json.loads(stations.HISTORICAL_GOLDEN.read_text())["games"][game]
    value = report(game)
    value["cases"] = copy.deepcopy(golden["cases"])
    value["dispatch_cases"] = copy.deepcopy(golden["dispatch_cases"])
    for row in value["cases"] + value["dispatch_cases"]:
        row["cleared_authored_robots"] = [
            [index, robot, index + 1] for index, robot in enumerate(row.pop("cleared_robot_types"))
        ]
        for index, birth in enumerate(row["births"]):
            birth.update(object=index, signature=index + 1)
    for row in value["dispatch_cases"]:
        row["mapping"] = copy.deepcopy(golden["mapping"])
        row["authored_triggers"] = copy.deepcopy(golden["authored_triggers"])
        row["activation_lights"] = [
            dict(segment=segment, signature=index + 1)
            for index, segment in enumerate(row.pop("activation_light_segments"))
        ]
    return value


class HistoricalMatcenTests(unittest.TestCase):
    def test_pinned_historical_traces(self):
        for game, count in (("d1", 16), ("d2", 24)):
            with self.subTest(game=game):
                self.assertEqual(stations.validate_historical_report(historical_report(game), game), count)

    def test_consistent_but_historically_wrong_reports_fail(self):
        # Every mutation passes the old semantic/self-consistency validator.
        # Matching a second execution of these bad results must not bless them.
        for game in ("d1", "d2"):
            for mutation in (
                "mask",
                "authored_segment",
                "dispatch_frame",
                "case_frame",
                "omitted_trigger",
                "trigger_order",
                "link_order",
            ):
                with self.subTest(game=game, mutation=mutation):
                    value = historical_report(game)
                    if mutation == "case_frame":
                        for row in value["cases"][:2]:
                            row["births"][0]["frame"] += 1
                    else:
                        for row in value["dispatch_cases"]:
                            if mutation == "mask":
                                row["mapping"][0]["robot_flags"][0] |= 1
                            elif mutation == "authored_segment":
                                row["authored_triggers"][0]["links"][0]["segment"] += 1
                            elif mutation == "dispatch_frame" and row["scenario"] in ("wall_dispatch", "sub"):
                                row["births"][-1]["frame"] += 1
                            elif mutation == "omitted_trigger":
                                row["authored_triggers"].pop()
                            elif mutation == "trigger_order":
                                row["authored_triggers"].reverse()
                            elif mutation == "link_order":
                                next(trigger for trigger in row["authored_triggers"] if len(trigger["links"]) > 1)[
                                    "links"
                                ].reverse()
                    stations.validate_report(value, game)
                    with self.assertRaisesRegex(stations.EvidenceError, "differs from bc19490fb"):
                        stations.validate_historical_report(value, game)

    def test_wrong_but_mask_permitted_robot_type_fails_history(self):
        value = historical_report("d2")
        for row in value["cases"][:2]:
            self.assertEqual(row["births"][0]["id"], 38)
            row["births"][0].update(id=39, shields=4259840)
        stations.validate_report(value, "d2")
        with self.assertRaisesRegex(stations.EvidenceError, "d2 cases differs"):
            stations.validate_historical_report(value, "d2")

    def test_official_d2_level_2_backlinks_must_not_reorder_masks(self):
        value = historical_report("d2")
        mapping = value["dispatch_cases"][0]["mapping"]
        self.assertEqual(mapping[0], dict(ordinal=0, station=5, segment=50, robot_flags=[0, 192]))
        self.assertEqual(mapping[2], dict(ordinal=2, station=16, segment=263, robot_flags=[33554432, 64]))
        provenance = json.loads(stations.HISTORICAL_GOLDEN.read_text())["provenance"]
        self.assertEqual(provenance["d2_level_2_disk_backlinks"], [263, 233, 50])
        self.assertEqual(provenance["d2_level_2_runtime_segments"], [50, 233, 263])
        for row in value["dispatch_cases"]:
            # A loader that follows authored disk backlinks instead of generated
            # ordinals swaps these two masks. Make births consistent with the
            # wrong masks to prove semantic validation alone cannot detect it.
            first, last = row["mapping"][0], row["mapping"][2]
            first["robot_flags"], last["robot_flags"] = last["robot_flags"], first["robot_flags"]
            for birth in row["births"]:
                if birth["ordinal"] == 0 and birth["id"] == 39:
                    birth.update(id=25, shields=5242880)
                elif birth["ordinal"] == 2 and birth["id"] == 25:
                    birth.update(id=39, shields=4259840)
        stations.validate_report(value, "d2")
        with self.assertRaisesRegex(stations.EvidenceError, "d2 mapping differs"):
            stations.validate_historical_report(value, "d2")

    def test_incidental_allocation_ids_do_not_change_historical_signature(self):
        for game in ("d1", "d2"):
            value = historical_report(game)
            for row in value["cases"] + value["dispatch_cases"]:
                for birth in row["births"]:
                    birth["object"] += 200
                    birth["signature"] += 300
                for cleared in row["cleared_authored_robots"]:
                    cleared[0] += 200
                    cleared[2] += 300
            for row in value["dispatch_cases"]:
                for light in row["activation_lights"]:
                    light["signature"] += 300
            stations.validate_historical_report(value, game)


if __name__ == "__main__":
    unittest.main()
