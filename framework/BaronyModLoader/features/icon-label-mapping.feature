Feature: Icon Label Mapping Contract
  As a BML GUI or store renderer,
  I need every OS, store, and runtime icon variant to carry a text label,
  so that accessibility, screen readers, and plain-text environments
  always have a meaningful fallback name for each icon.

  Background:
    Given the BML Python app module path

  @bdd-harness
  Scenario: App-core defines an icon-label mapping for all OS/store/runtime variants
    When I run a Python script that checks for an icon label mapping constant
    Then the module defines ICON_LABELS or an equivalent dict
    And every entry in the mapping has a non-empty text label

  @bdd-harness
  Scenario: Icon label mapping covers OS runtime icons
    When I run a Python script that checks OS icon labels
    Then entries exist for: linux, windows, darwin icon variants

  @bdd-harness
  Scenario: Icon label mapping covers store (Steam) icons
    When I run a Python script that checks store icon labels
    Then entries exist for: workshop thumbnail, library grid icon variants
