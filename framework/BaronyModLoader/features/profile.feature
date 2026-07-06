Feature: Profile BDD Phase Contract
  As the BaronyModLoader app-core profile phase,
  I need profile state exposed as semantic DTOs,
  so the GUI can render profile readiness without parsing raw CLI stdout or touching native/runtime/product systems.

  Background:
    Given a hermetic Profile BDD workspace with a fake Barony executable
    And the Profile BDD contract imports the BaronyModLoader Python app module

  @profile-bdd @profile-create
  Scenario: Stable profile creation returns semantic profile state
    When I create a Profile BDD profile named "stable-profile"
    Then the Profile BDD state exposes a stable profile id and profile-local paths

  @profile-bdd @profile-reload
  Scenario: Profile reload does not rewrite the profile store
    When I reload a Profile BDD profile without changing it
    Then the Profile BDD reload reports unchanged state and no profile rewrite

  @profile-bdd @profile-active-count
  Scenario: Active mod count is projected from semantic profile state
    When I enable the Runebound package for a Profile BDD profile
    Then the Profile BDD state projects an active mod count of 1

  @profile-bdd @profile-stale-mods
  Scenario: Stale active mods are surfaced as semantic warnings
    When I load a Profile BDD profile with a stale active Runebound entry
    Then the Profile BDD state warns about stale active mods semantically

  @profile-bdd @profile-idempotence
  Scenario: Enable and disable operations are idempotent state transitions
    When I enable and disable the same Profile BDD mod repeatedly
    Then the Profile BDD mutation results are idempotent semantic state transitions

  @profile-bdd @profile-no-tmp
  Scenario: Product paths never point at a .tmp profile location
    When I inspect the Profile BDD product paths for a created profile
    Then no Profile BDD product path uses a .tmp location
