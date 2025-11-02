#ifndef SECURITY_TESTS_H
#define SECURITY_TESTS_H

#include <switch.h>
#include "crypto.h"
#include "secure_validation.h"
#include "security_audit.h"

// Test categories
typedef enum {
    TEST_CRYPTO,
    TEST_VALIDATION,
    TEST_AUDIT,
    TEST_UI_SECURITY,
    TEST_FILE_SECURITY,
    TEST_MEMORY_SECURITY
} TestCategory;

// Test result structure
typedef struct {
    const char *test_name;
    const char *test_description;
    TestCategory category;
    bool passed;
    char error_message[256];
    double duration_ms;
} TestResult;

// Test suite structure
typedef struct {
    TestResult *results;
    size_t test_count;
    size_t passed_count;
    size_t failed_count;
    double total_duration_ms;
} TestSuite;

// Initialize test framework
Result test_init(void);
void test_exit(void);

// Core test functions
Result test_run_all(TestSuite *suite);
Result test_run_category(TestSuite *suite, TestCategory category);
Result test_run_single(TestSuite *suite, const char *test_name);

// Cryptographic tests
void test_crypto_key_derivation(TestSuite *suite);
void test_crypto_file_encryption(TestSuite *suite);
void test_crypto_memory_encryption(TestSuite *suite);
void test_crypto_authenticated_encryption(TestSuite *suite);
void test_crypto_random_generation(TestSuite *suite);

// Validation tests
void test_file_validation(TestSuite *suite);
void test_nsp_validation(TestSuite *suite);
void test_nca_validation(TestSuite *suite);
void test_input_validation(TestSuite *suite);
void test_install_validation(TestSuite *suite);

// Audit tests
void test_audit_filesystem(TestSuite *suite);
void test_audit_crypto(TestSuite *suite);
void test_audit_memory(TestSuite *suite);
void test_audit_reporting(TestSuite *suite);

// UI security tests
void test_security_prompts(TestSuite *suite);
void test_security_indicators(TestSuite *suite);
void test_validation_ui(TestSuite *suite);

// Memory security tests
void test_secure_memory_wipe(TestSuite *suite);
void test_memory_protection(TestSuite *suite);
void test_buffer_overflow_protection(TestSuite *suite);

// Utility functions
void test_suite_init(TestSuite *suite);
void test_suite_free(TestSuite *suite);
Result test_save_results(const TestSuite *suite, const char *path);
void test_print_results(const TestSuite *suite);
const char* test_category_string(TestCategory category);

#endif // SECURITY_TESTS_H