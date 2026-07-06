Feature: Headless App-Core Isolation
  As a BML launcher or CI harness,
  I need the headless app-core to run without GUI dependencies,
  so that profile operations, package management, and runtime registration
  work in SSH sessions, containers, and SteamCMD environments.

  Background:
    Given the BML Python app module path

  @bdd-harness
  Scenario: App-core imports succeed without tkinter
    When I run a Python script that imports the BML app module
    Then the import succeeds without raising ImportError
    And the process exits cleanly (exit code 0)

  @bdd-harness
  Scenario: Headless guard — app-core must not pull in tkinter
    When I run a Python script that checks whether the BML app module imports tkinter
    Then the check reports that tkinter is NOT imported by app-core
    And the process exits cleanly (exit code 0)

  @bdd-harness
  Scenario: App-core module has no tkinter dependency at the source level
    When I inspect the source of the BML app module for import statements
    Then no import of the "tkinter" module is found in the source code
    And no "from tkinter" relative-import is found in the source code
