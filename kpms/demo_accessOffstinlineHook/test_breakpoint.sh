#!/system/bin/sh
# Hardware Breakpoint Test Script
# Tests the freeze fix for hardware breakpoints

SUPERKEY="pengxintu123"
CONTROL="/data/local/tmp/kpm_control"

echo "=========================================="
echo "Hardware Breakpoint Freeze Fix Test"
echo "=========================================="
echo ""

# Check if kpm_control exists
if [ ! -f "$CONTROL" ]; then
    echo "Error: kpm_control not found at $CONTROL"
    echo "Please build and push it first"
    exit 1
fi

echo "Step 1: Check module status"
echo "------------------------------------------"
$CONTROL $SUPERKEY get_status
echo ""

echo "Step 2: Clear any existing breakpoints"
echo "------------------------------------------"
$CONTROL $SUPERKEY bp_clear_all
echo ""

echo "Step 3: List breakpoints (should be empty)"
echo "------------------------------------------"
$CONTROL $SUPERKEY bp_list
echo ""

echo "Step 4: Set a test breakpoint"
echo "------------------------------------------"
echo "Note: Using a dummy address for testing"
echo "In real use, replace with actual function address"
$CONTROL $SUPERKEY bp_set 0x7f12345678 0 2 test_function
echo ""

echo "Step 5: List breakpoints (should show 1)"
echo "------------------------------------------"
$CONTROL $SUPERKEY bp_list
echo ""

echo "Step 6: Check verbose mode (should be OFF by default)"
echo "------------------------------------------"
$CONTROL $SUPERKEY get_status | grep verbose
echo ""

echo "=========================================="
echo "Test Setup Complete"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Trigger the breakpoint by executing code at that address"
echo "2. Check kernel log: dmesg | grep HW_BP"
echo "3. Verify program doesn't freeze"
echo "4. Check hit count: $CONTROL $SUPERKEY bp_list"
echo ""
echo "To test verbose mode (use with caution):"
echo "  $CONTROL $SUPERKEY bp_verbose_on"
echo "  # Trigger breakpoint ONCE"
echo "  $CONTROL $SUPERKEY bp_verbose_off"
echo ""
echo "To clean up:"
echo "  $CONTROL $SUPERKEY bp_clear_all"
echo ""
