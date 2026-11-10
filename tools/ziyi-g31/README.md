# G31 CPU observer for guarded SQE start

One native source scope. Engine, command packet, profile, all permissions and the 15-second engine bound remain unchanged. The same single device STR and original DSB are bracketed by three aligned CPU progress stores, with no printf, sleep, extra MMIO or new barrier between them. Stage1 is immediately before STR, stage2 is after CPU store issue (not a device ACK), stage3 is after the original DSB returns (not readback).

A separate CPU-bound observer prints a strict128-byte GSR1 record. Worker and observer must report distinct CPU IDs before the packet can run. SQE byte contents, private binding addresses and CP receipts are checked before the original final live owner/current/store predicates run again. Observer reads only immutable CPU metadata and an atomic progress field. Its budget is60seconds/1200samples; no hardware access or cleanup occurs in the observer. Failed setup parks with all borrowers retained.

This is a diagnostic, not GPU execution proof. Preserve G21 software rollback and require a new full qualification and fresh cold fastboot before one hardware run.
