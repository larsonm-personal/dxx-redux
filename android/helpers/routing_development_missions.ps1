function Get-RoutingDevelopmentMissions {
    return @(
        [pscustomobject]@{ Json = 'castaway_redux.json'; Archive = 'castaway_redux.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'Counterstrike.json'; Archive = ''; BuiltInGame = 'd2' },
        [pscustomobject]@{ Json = 'FirstStrike.json'; Archive = ''; BuiltInGame = 'd1' },
        [pscustomobject]@{ Json = 'Obsidian.json'; Archive = 'Obsidian.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'TEW.json'; Archive = 'TEW.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'plutonia.json'; Archive = 'plutonia.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'CD - Descent II - The Vertigo Series (USA).json'; Archive = ''; BuiltInGame = ''; CdSourceId = 'descent-ii-vertigo-usa' },
        [pscustomobject]@{ Json = 'Vignettes.json'; Archive = 'Vignettes.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'Entropy2.json'; Archive = 'Entropy2.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'descent_maximum_fixed.json'; Archive = 'descent_maximum_fixed.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'af_d1_beta.json'; Archive = 'af_d1_beta.zip'; BuiltInGame = '' },
        [pscustomobject]@{ Json = 'Mandrill.json'; Archive = 'Mandrill.zip'; BuiltInGame = '' }
    )
}
