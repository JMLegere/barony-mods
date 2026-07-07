Feature: Profile-first GUI Mods list
  As a BaronyModLoader user,
  I need the main body to put the Mods list first and group rows by provenance,
  so I can select local and Workshop mods while still seeing active profile state on local rows.

  Background:
    Given a completed profile-first GUI smoke report

  @gui-profile-first @provenance-sidebar @first-launch
  Scenario: Main body uses a Mods left list instead of concept cards
    When the user opens the profile-first mod manager for the first time
    Then the left side of the main body is a Mods list
    And the Mods list is titled Mods
    And the Mods list header exposes scan or filter list controls
    And the Mods list is not a concept-card column
    And the right side shows selected mod details with contextual actions
    And the right side keeps compact Environment, Profile, Workshop status cards and Recent Activity
    And the compact Workshop status card shows dry-run no-publish state without primary blocker dumps
    And visible Recent Activity entries are concise, target-named, and keep verbose data in report details
    And the right side does not keep a separate Mods card

  @gui-profile-first @copy-for-ai @activity-log
  Scenario: Recent Activity exposes Copy for AI support context action outside status cards
    When the user opens the profile-first mod manager for the first time
    Then the Recent Activity area exposes a Copy for AI button
    And Copy for AI is not exposed as an Environment, Profile, or Workshop compact status card action


  @gui-profile-first @entity-iconography
  Scenario: Major profile-first entities expose paired iconography
    When the user reviews the profile-first entity iconography
    Then the smoke report exposes rendered entity iconography for every major entity
    And Steam entity iconography keeps logo evidence or clear fallback text
    And renderedEntityIcons pairs every icon with accessible text labels

  @gui-profile-first @provenance-sidebar @sections
  Scenario: Mods are sectioned by provenance
    When the user scans the Mods list
    Then the smoke report exposes detectedModSections
    And the rendered provenance section labels include local repo and Workshop groups without enabled profile duplicates
    And Runebound: Elixirs appears under local repo provenance
    And active profile state is represented on the local repo row


  @gui-profile-first @mods-list @state-prefixes
  Scenario: Mods list rows render aligned enabled and disabled state prefixes
    When the user scans the Mods list
    Then the smoke report exposes renderedDetectedModRows
    And enabled Mods rows render a green check prefix in the aligned prefix column
    And disabled Mods rows reserve the aligned prefix column and include disabled, detected, or subscribed state text
    And Mods rows expose keyboard focus and navigation metadata

  @gui-profile-first @mods-list @selection @selected-mod
  Scenario: Selecting a local enabled BML mod row exposes canonical selectedMod state
    When the smoke selects Runebound: Elixirs from the Mods list
    Then the smoke report exposes selectable Mods row metadata
    And selectedDetectedMod changes to the requested smoke-selected row
    And selected Mods row exposes visible focus or selection affordance
    And the selected local BML package details match Runebound: Elixirs
    And the canonical selectedMod matches enabled local Runebound: Elixirs
    And selectedMod exposes Disable selected mod as the primary action and disables redundant Enable selected mod
    And the selected mod detail panel matches selectedMod and exposes detail actions
    And selecting the mod uses scoped redraw metadata instead of a full dashboard redraw

  @gui-profile-first @mods-list @selection @selected-mod
  Scenario: Selecting a local disabled BML mod row exposes canonical selectedMod state
    When the smoke selects Stash from the Mods list
    Then the smoke report exposes selectable Mods row metadata for Stash
    And selectedDetectedMod changes to the requested Stash row
    And selected Mods row exposes visible focus or selection affordance
    And the canonical selectedMod matches disabled local Stash
    And selectedMod exposes Enable selected mod as the primary action and disables redundant Disable selected mod
    And the selected mod detail panel matches selectedMod and exposes detail actions
    And selecting the mod uses scoped redraw metadata instead of a full dashboard redraw

  @gui-profile-first @mods-list @selection @stash-targeting @selected-mod
  Scenario: Enabling a selected disabled Stash row targets Stash
    When the smoke selects Stash from the Mods list and clicks Enable selected mod
    Then the smoke report exposes selectable Mods row metadata for Stash
    And selectedDetectedMod changes to the requested Stash row
    And the canonical selectedMod identifies Stash as the Enable selected mod target
    And Enable selected mod targets Stash and does not reuse Runebound state

  @gui-profile-first @provenance-sidebar @workshop
  Scenario: Steam Workshop subscriptions can be listed without BML package metadata
    When the user scans the Mods list
    Then Steam Workshop subscriptions appear under Workshop provenance when detected
    And the Workshop provenance section does not require subscriptions to be BML packages
    And the Workshop concept exposes no Steam publish side effects

  @gui-profile-first @mods-list @selection @workshop @selected-mod
  Scenario: Selecting a non-BML Workshop subscription exposes safe canonical selectedMod state
    When the smoke selects the Workshop subscription from the Mods list
    Then the smoke report exposes selectable Mods row metadata for the Workshop subscription
    And the canonical selectedMod matches the Workshop subscription
    And selectedMod marks package enable and disable actions unavailable for the Workshop subscription
    And selected Mods row exposes visible focus or selection affordance
    And the selected mod detail panel matches selectedMod and exposes detail actions
    And selecting the mod uses scoped redraw metadata instead of a full dashboard redraw
    And the Workshop concept exposes no Steam publish side effects

  @gui-profile-first @environment
  Scenario: Environment summarizes OS, Steam platform, and Game version with icon evidence
    When the user reviews the Environment card
    Then Environment summary rows include OS, Platform, and Game version
    And the Platform row value is Steam storefront instead of linux-x86_64
    And the Platform row renders as Platform: Steam, not Steam: Steam
    And the Environment smoke report proves the Steam logo is rendered
    And Environment summary rows pair Platform, OS, and Game version values with logo or icon metadata
    And Environment compact status actions expose Launch BML Barony and Launch Vanilla Barony
    And the smoke report includes full Environment launch action labels and Workshop warning text
    And hidden smoke window metadata proves no widget was auto-focused
    And important labels, buttons, and warnings expose no-clipping metadata

    And diagnostic details remain available in smoke report details fields

  @gui-profile-first @playability-boundary
  Scenario: GUI never claims Runebound is playable
    When the user reviews the Mods list for Runebound evidence
    Then the GUI makes no playable claim for Runebound: Elixirs
