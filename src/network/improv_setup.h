/*
 * CYD_AnimatedPixelClock - Improv-Serial WiFi Provisioning
 *
 * Improv-Serial over the CYD's USB bridge, listening on every boot - not only
 * on a fresh device as upstream does. ESP Web Tools uses it two ways:
 *
 *  - After an install it waits up to 10s for Improv and offers "Configure
 *    WiFi", pushing the credentials over USB.
 *  - On Connect it asks for device info within 1.5s. A reply naming this
 *    firmware (PROJECT_NAME) makes it offer "Update" - written without an
 *    erase, so settings survive - instead of "Install" and an erase prompt.
 *    It also offers "Visit device" and "Change WiFi".
 *
 * So the listener must answer from the setup portal loop and from loop(). It
 * costs nothing when no browser is attached; the WiFiManager AP portal runs in
 * parallel on a device with no WiFi.
 *
 * Enabled via IMPROV_SETUP_ENABLED in include/config.h.
 */

#ifndef IMPROV_SETUP_H
#define IMPROV_SETUP_H

#include "config.h"

#if IMPROV_SETUP_ENABLED

// Set the device info Improv reports. Call once, early in setup().
void improvBegin();

// Service Improv-Serial: from the setup portal loop and every loop() pass.
// When new credentials arrive and connect, they are already saved; this logs
// it and restarts so the device comes up cleanly on the new network. If they
// fail, the previous network is restored.
void improvTick();

#else

inline void improvBegin() {}
inline void improvTick() {}

#endif // IMPROV_SETUP_ENABLED
#endif // IMPROV_SETUP_H
