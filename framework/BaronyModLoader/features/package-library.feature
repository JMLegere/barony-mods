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

  @package-library @multiple-active-modlist
  Scenario: Multiple active packages produce launchable modlist state
    Given a Package Library temp profile with multiple active packages
    When I ask the Package Library API to evaluate the compatible multi-active modlist state
    Then the Package Library represents multiple active packages as launchable modlist state

  @package-library @compatible-multi-mod
  Scenario: Compatible multi-mod profile is accepted for launch
    Given a Package Library temp profile with compatible staged active packages
    When I ask the Package Library API to evaluate the staged modlist compatibility state
    Then the Package Library compatibility plan accepts the multi-mod launch

  @package-library @dependency-error-blocking
  Scenario: Missing required package dependency blocks launch
    Given a Package Library temp profile with a staged package requiring a missing package
    When I ask the Package Library API to evaluate the staged modlist compatibility state
    Then the Package Library compatibility plan blocks launch for the missing required dependency

  @package-library @warning-only-launchable
  Scenario: Missing optional dependency remains launchable with visible warning
    Given a Package Library temp profile with a staged package missing optional compatibility targets
    When I ask the Package Library API to evaluate the staged modlist compatibility state
    Then the Package Library compatibility plan remains launchable with visible compatibility warnings

  @package-library @deterministic-load-order
  Scenario: Compatible active packages produce deterministic load order
    Given a Package Library temp profile with staged packages that declare load ordering
    When I ask the Package Library API to evaluate the staged modlist compatibility state
    Then the Package Library compatibility plan returns the deterministic staged load order
