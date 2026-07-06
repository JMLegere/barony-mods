Feature: Profile-first GUI button interactions
  As a BaronyModLoader user,
  I need visible GUI buttons to perform real, validated actions,
  so the profile-first launcher feels useful and trustworthy instead of inert.

  Background:
    Given a BaronyModLoader GUI button interaction smoke report path
    When I run the BaronyModLoader GUI with all smoke button clicks

  @gui-button-interactions @environment @dry-run
  Scenario: Environment buttons update visible readiness, diagnostics, and launch dry-run feedback
    Then the button smoke invoked the Environment button actions
    And Environment button feedback updates readiness, diagnostics, and launch dry-run activity without starting Barony

  @gui-button-interactions @profiles
  Scenario: Profiles button creates and selects a stable profile outside .tmp
    Then the Profiles create-select-profile action creates and selects a stable profile outside .tmp

  @gui-button-interactions @mods @scan
  Scenario: Mods scan button lists Runebound
    Then the Mods scan-packages action lists Runebound: Elixirs

  @gui-button-interactions @mods @active-state
  Scenario: Mods enable and disable buttons mutate active mod state and are idempotent
    Then the Runebound enable and disable button clicks mutate active mod state
    And the Runebound enable and disable button clicks are idempotent

  @gui-button-interactions @workshop @dry-run
  Scenario: Workshop button produces a dry-run no-publish preview with no Steam side effects
    Then the Workshop preview action produces a dry-run no-publish preview
    And no Steam publish side effects are reported

  @gui-button-interactions @live-smoke
  Scenario: Live smoke invokes actual Tk buttons and reports clicked actions plus visible activity
    Then the smoke report proves actual Tk buttons were invoked
    And the smoke report includes clickedActions and a visible activity log for every expected button action

  @gui-button-interactions @playability-boundary
  Scenario: GUI button actions do not claim Runebound is playable
    Then no GUI button action claims Runebound: Elixirs is playable
