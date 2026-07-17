package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class LevelMetadataRouteNumberingTest {
    @Test
    fun startIsUnnumberedAndObjectivesBeginAtOne() {
        val steps =
            listOf(
                LevelMetadataRouteStep(index = 0, kind = "start", label = "start"),
                LevelMetadataRouteStep(
                    index = 1,
                    kind = "key",
                    activationKind = "pickup_key",
                    label = "blue key",
                    key = "blue",
                ),
                LevelMetadataRouteStep(
                    index = 2,
                    kind = "trigger",
                    activationKind = "shoot_switch",
                    label = "shoot trigger 6",
                    trigger = 6,
                ),
                LevelMetadataRouteStep(
                    index = 3,
                    kind = "exit",
                    activationKind = "enter_exit",
                    label = "exit",
                ),
            )

        assertEquals(
            listOf("start", "1. Collect Blue key", "2. Shoot switch", "3. Enter exit"),
            levelMetadataRouteStepHeadings(steps),
        )
    }

    @Test
	fun numberingStillBeginsAtOneWhenAStartRowIsUnavailable() {
        val steps =
            listOf(
                LevelMetadataRouteStep(index = 0, kind = "reactor", label = "Reactor"),
                LevelMetadataRouteStep(index = 1, kind = "exit", label = "Exit"),
            )

        assertEquals(
            listOf("1. Reactor", "2. Exit"),
            levelMetadataRouteStepHeadings(steps),
        )
	}

	@Test
	fun unresolvedObjectiveIsFlaggedWithoutChangingNumbering() {
		val steps =
			listOf(
				LevelMetadataRouteStep(index = 0, kind = "start", label = "Start"),
				LevelMetadataRouteStep(
					index = 1,
					kind = "trigger",
					activationKind = "unresolved_trigger",
					calculated = false,
					label = "Locate and activate switch trigger 8",
				),
				LevelMetadataRouteStep(index = 2, kind = "exit", label = "Exit"),
			)

		assertEquals(
			listOf(
				"Start",
				"1. Locate and activate switch trigger 8 (Not calculated)",
				"2. Exit",
			),
			levelMetadataRouteStepHeadings(steps),
		)
	}
}
