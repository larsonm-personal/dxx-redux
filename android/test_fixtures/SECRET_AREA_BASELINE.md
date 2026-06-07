# Secret Area Baseline

`secret_area_base_game_baseline.json` is generated from the base Descent and Descent 2 game assets by:

```powershell
.\android\tests\update_secret_area_baseline.ps1 -RequireAssets
```

Review scanner algorithm changes before updating this file. The fixture is meant to flag changes in generated secret counts, ordering, entry walls, labels, and segment membership.
