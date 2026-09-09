function Get-RoutingDevelopmentMissions {
    return @(
        [pscustomobject]@{ Json = 'castaway_redux.json'; Archive = 'castaway_redux.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'Counterstrike.json'; Archive = ''; BuiltInGame = 'd2' },
        [pscustomobject]@{ Json = 'FirstStrike.json'; Archive = ''; BuiltInGame = 'd1' },
        [pscustomobject]@{ Json = 'Obsidian.json'; Archive = 'Obsidian.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'TEW.json'; Archive = 'TEW.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'plutonia.json'; Archive = 'plutonia.zip'; BuiltInGame = '' }
    )
}
