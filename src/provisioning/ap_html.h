#ifndef AP_HTML_H
#define AP_HTML_H

#include <Arduino.h>

// ===============================
// PROVISIONING HTML PAGES
// ===============================

/**
 * Get main setup page HTML
 * Returns complete HTML document with embedded CSS/JS
 * Mobile-first responsive design
 * 
 * @return HTML string for setup page
 */
const char* getSetupPageHTML();

/**
 * Get success page HTML
 * Shown after successful configuration save
 * 
 * @return HTML string for success page
 */
const char* getSuccessPageHTML();

#endif