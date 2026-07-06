Feature: Command Result DTO Contract
  As a BML GUI layer or external caller,
  I need command execution results to expose a well-structured DTO,
  so I can render status, render stdout/stderr, and expose
  exit_code, argv, duration, and failure summary without GUI parsing.

  Background:
    Given the BML Python app module path

  @bdd-harness
  Scenario: Package validate returns structured output with exit code and problems
    When I invoke the CLI command "package validate mods/stash" on the example stash package
    Then the command exits with code 0
    And the stdout output does not contain error problem codes prefixed with "BML_"

  @bdd-harness
  Scenario: Package validate of a malformed package surfaces problems
    When I invoke the CLI command "package validate /nonexistent-path-12345" on a path that does not exist
    Then the command exits with non-zero code
    And the stdout output contains a problem code starting with "BML_"

  @bdd-harness
  Scenario: Command result DTO preserves argv, exit code, stdout, stderr
    When I invoke the CLI command "package validate mods/stash" on the example stash package
    Then the CLI emits the command name in the output header
    And the CLI reports exit code 0 in the output

  @bdd-harness
  Scenario: App-core exposes a CommandResult or equivalent DTO dataclass
    When I run a Python script that checks for a CommandResult-like dataclass
    Then the module defines a result DTO with fields: argv, label, exit_code, stdout, stderr, duration, failure_summary
