# EPICS Real-Time Diagnostic Monitor (threadExample)

## Overview
A high-performance, non-blocking diagnostic shell command (`threadExample`) designed for EPICS (Experimental Physics and Industrial Control System) Input/Output Controllers (IOCs). This tool allows operators to perform computationally heavy diagnostic checks on live Process Variables (PVs) directly from the IOC shell without influencing or delaying the core PV processing sequence.

## Core Features & Architecture
In industrial and scientific control systems, diagnostic tools must maintain strict real-time constraints. This project implements a **3-Tier Multithreading Architecture** to ensure zero-latency operation:

* **Zero PV Interference:** The diagnostic routine runs entirely independent of the main EPICS scan threads.
* **dbChannel API Integration:** Establishes direct, internal connections to PV records bypassing CA-network overhead.
* **Thread-Safe Event Handling:** Utilizes POSIX-style synchronization (`epicsMutex`, `epicsEvent`) to securely pass PV data from the high-priority event callback to a low-priority worker thread.
* **Step-by-Step Processing:** Guarantees that every PV update is processed in real-time without skipping or dropping data frames.

## The Diagnostic Task
The specific use case implemented in this repository monitors a `compress` record (acting as a circular buffer). Upon every data update, the tool:
1. Calculates the sum of the entire VAL-array.
2. Performs a prime number validation on the sum.
3. Outputs an ISO-8601 timestamp if the sum is a prime number.

## Technologies Used
* **Language:** C
* **Framework:** EPICS Base (≥ 7.0.4)
* **Concepts:** Real-Time Systems, Multithreading, Thread Synchronization, dbChannel API, Distributed Control Systems.

## Example Usage (IOC Shell)
```bash
epics> help threadExample
threadExample pv-name
Check on a given compress record that the sum of val-array is prime
Output of the timestamp if prim. The calculation must not influence the PV process.
Example: threadExample junkes:compressExample

epics> threadExample $(USERNAME):compressExample
threadExample: '$(USERNAME):compressExample' NSAM = 3333
threadExample: subscription active for '$(USERNAME):compressExample'
New worker threadId: 0x7b8b4c00
sum = 163
worker data is prim 163 at 2026-06-14T16:31:02.762457000Z
