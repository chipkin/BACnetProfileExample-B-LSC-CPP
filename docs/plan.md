# Plan (STUB): B-LSC (Life Safety Controller) — C++ example

> **STATUS: STUB.** Seed facts below. Expand from
> [`bacnet-profile-plan-template.md`](../../bacnet-profile-plan-template.md) after the
> sample plans ([B-LD](../../BACnetProfileExample-B-LD-CPP/docs/plan.md),
> [B-BC](../../BACnetProfileExample-B-BC-CPP/docs/plan.md)) are reviewed.

**Profile:** B-LSC · **Family:** Annex L.5 (Life Safety Controller) · **Role:** B ·
**Archetype:** Controller · **Difficulty:** 4/5 · **Build wave:** 3

**Thesis:** a life-safety controller — fire/smoke panel logic with **Life Safety
Point / Zone** objects that generate **CHANGE_OF_LIFE_SAFETY** alarms. Builds on
B-AAC's alarming. Canonical source for **F-REINIT, F-LIFESAFETY, F-ALARM-LS**.

## Required BIBBs (profiles.md L.5)
`DS-RP-B, DS-RPM-B, DS-WP-B, DS-WPM-B, DS-COV-B; AE-LS-B, AE-ACK-B, AE-INFO-B;
DM-DDB-A,B, DM-DOB-B, DM-DCC-B, (DM-TS-B or DM-UTC-B), DM-RD-B`.

## Services to enable
- RP (1), RPM (14), WP (15), WPM (16), SubscribeCOV (5), DCC (17), TimeSync
  (24/25), ReinitializeDevice (20), GetEventInformation (39), AcknowledgeAlarm.

## Objects (baseline + )
- Life Safety Point 1, Life Safety Zone 1 (`Mode`, `Operation_Expected`,
  `Life_Safety_Alarm_Values`, ... — confirm in DLL), Notification Class 1.

## Shared features
- **DEFINE:** F-REINIT (DM-RD-B — first ReinitializeDevice), F-LIFESAFETY (LSP/LSZ
  objects), F-ALARM-LS (`SetIntrinsicChangeOfLifeSafetyAlgorithm`).
- **REUSE:** F-ALARM (B-AAC), F-COV (B-ACCR), F-TIMESYNC (B-LD), F-DCC (B-ASC),
  F-OUTPUTS (B-SA).

## Known stack gaps
- EventNotification recipient by **address** workaround (F-ALARM; master plan §7
  risk 2). profiles.md: ✅ S67 (AE-LS-B via CHANGE_OF_LIFE_SAFETY algorithm,
  151-test panel PASS).

## Notes / open questions
- Build after B-ACCR (F-COV) and B-LD (F-TIMESYNC). Confirm the LSP/LSZ
  required-property set and the life-safety event-state model empirically.
