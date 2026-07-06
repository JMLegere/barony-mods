Feature: GUI Binding app-core contracts
  As the BaronyModLoader GUI shell,
  I need pure semantic binding/view-model contracts,
  so the app can render one headless-safe page without scraping CLI text or requiring a display.

  Background:
    Given a headless GUI Binding staging directory
    And the GUI binding app-core module path

  @gui-binding @app-core
  Scenario: One-page shell exposes every semantic section
    When I probe the GUI one-page shell binding contract
    Then the shell binding exposes one page with install, profile, package, readiness, diagnostics, and workshop sections

  @gui-binding @app-core
  Scenario: Widgets bind to semantic dashboard state
    When I probe the GUI widget semantic binding contract
    Then the widget bindings expose stable widget ids backed by dashboard semantic paths

  @gui-binding @app-core
  Scenario: Icon bindings carry text for accessible rendering
    When I probe the GUI icon text accessibility binding contract
    Then every surfaced GUI icon has accessible text

  @gui-binding @app-core
  Scenario: Command failure details survive GUI binding
    When I execute a failing command through the GUI command result contract
    Then the command result preserves argv, exit code, stdout, stderr, and failure summary

  @gui-binding @app-core
  Scenario: Slow commands have a non-blocking pending view state
    When I probe the GUI slow command responsiveness contract
    Then the command binding exposes a pending state without blocking for command completion

  @gui-binding @app-core
  Scenario: Disabled actions explain why they cannot run
    When I probe the GUI disabled action reason contract
    Then disabled GUI actions include specific user-facing reasons
