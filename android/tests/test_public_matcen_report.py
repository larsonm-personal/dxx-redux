"""The station validator rejects empty fixtures and vacuous suppression passes."""

import copy
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
    return dict(
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


if __name__ == "__main__":
    unittest.main()
