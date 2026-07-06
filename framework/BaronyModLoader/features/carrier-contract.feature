Feature: Runebound Elixirs Carrier Contract (POTION_STRENGTH)
  As a BML runtime author and a Runebound: Elixirs package consumer,
  I need the app-core and schema to accept POTION_STRENGTH as the elixir
  carrier item type,
  so that the live hook metadata, drop generation, and consumption logic
  all agree on which Barony item type carries authored elixir metadata.

  This is the Preflight Carrier Contract test: the BML validation pipeline
  (app constant, JSON schema, runtime manifest schema) must align on
  POTION_STRENGTH as the canonical carrier. The real package at
  mods/runebound-elixirs/bml-package.json uses POTION_STRENGTH.
  The native hook smoke (assert_elixir_production_validation.py) and
  run_elixir_production_validation.sh both expect POTION_STRENGTH.
  Any mention of POTION_EMPTY in app/schema is a regression.

  Background:
    Given the BML Python app module path
    And the package schema path
    And the runtime manifest schema path
    And the real Runebound: Elixirs package path

  @carrier-contract
  Scenario: App constant RUNEBOUND_ELIXIRS_CARRIER_ITEM_TYPE is POTION_STRENGTH
    When I run a Python script that asserts the app constant equals "POTION_STRENGTH"
    Then the assertion succeeds
    And the constant is the string "POTION_STRENGTH"

  @carrier-contract
  Scenario: Package schema allows POTION_STRENGTH for carrierItemType
    When I validate the package schema with carrierItemType set to "POTION_STRENGTH"
    Then the schema validation passes

  @carrier-contract
  Scenario: Runtime manifest schema allows POTION_STRENGTH for carrierItemType
    When I validate the runtime manifest schema with carrierItemType set to "POTION_STRENGTH"
    Then the schema validation passes

  @carrier-contract
  Scenario: BML package validate accepts the real Runebound: Elixirs package
    When I invoke the CLI command "package validate" on the real runebound-elixirs package
    Then the command exits with code 0
    And the stdout output does not contain "BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID"

  @carrier-contract
  Scenario: BML package validate rejects POTION_EMPTY in the real package
    When I invoke the CLI command "package validate" on the real runebound-elixirs package
    Then if the package uses "POTION_EMPTY" as carrierItemType, the validation fails with BML_PACKAGE_RUNEBOUND_ELIXIRS_MODULE_FIELD_INVALID

  @carrier-contract
  Scenario: Native hook smoke expects POTION_STRENGTH carrier metadata
    When the native elixir smoke test is run
    Then the recognizedCarrier.carrierItemType equals "POTION_STRENGTH"
    And the dropGeneration.carrierItem equals "POTION_STRENGTH"

  @carrier-contract
  Scenario: No code path in app-core or schema references POTION_EMPTY as elixir carrier
    When I search all Python source files under the BML app directory
    Then no reference to "POTION_EMPTY" appears in a runebound elixir context
