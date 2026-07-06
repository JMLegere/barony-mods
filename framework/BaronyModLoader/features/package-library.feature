Feature: Package Library Contract
  As the BaronyModLoader app shell,
  I need a semantic package library for local mods and profile activation,
  so the GUI can render package cards and enforce active-package rules without scraping CLI text.

  Background:
    Given a clean Package Library staging directory

  @package-library @local-scan
  Scenario: Local mod package scan
    When I ask the Package Library API to scan a local Runebound package root
    Then the Package Library scan returns a semantic card for Runebound Elixirs

  @package-library @validation-card
  Scenario: Package validation failure card
    When I ask the Package Library API to scan a malformed package fixture
    Then the Package Library returns a validation failure card with structured problems

  @package-library @profile-enable
  Scenario: Package enable with profile
    Given a Package Library temp profile with a fake Barony executable
    When I ask the Package Library API to enable Runebound Elixirs for that profile
    Then the profile semantic active package state contains Runebound Elixirs as enabled

  @package-library @profile-disable
  Scenario: Package disable preserves files
    Given a Package Library temp profile with a fake Barony executable
    And Runebound Elixirs is installed and enabled from a Package Library store
    When I ask the Package Library API to disable Runebound Elixirs for that profile
    Then Runebound Elixirs is inactive and its installed package files remain on disk

  @package-library @duplicate-versions
  Scenario: Duplicate package versions
    When I ask the Package Library API to scan two Runebound Elixirs versions
    Then the Package Library returns one semantic card per Runebound version without clobbering paths

  @package-library @multiple-active-guard
  Scenario: Multiple active package guard
    Given a Package Library temp profile with multiple active packages
    When I ask the Package Library API to evaluate the active-package guard
    Then the Package Library blocks profiles with multiple active packages before launch
