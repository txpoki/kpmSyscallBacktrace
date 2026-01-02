#!/system/bin/sh
# Test different KernelPatch versions to find the correct one

SUPERKEY="$1"
if [ -z "$SUPERKEY" ]; then
    echo "Usage: $0 <superkey>"
    exit 1
fi

echo "Testing KernelPatch versions..."
echo "================================"

# Test common versions
for major in 0; do
    for minor in 9 10 11; do
        for patch in 0 1 2 3 4 5 6 7 8 9; do
            VERSION="${major}.${minor}.${patch}"
            
            # Update version file
            cat > /data/local/tmp/test_version << EOF
#define MAJOR ${major}
#define MINOR ${minor}
#define PATCH ${patch}
EOF
            
            echo -n "Testing version ${VERSION}... "
            
            # This would require recompiling, so just show the version
            # In practice, you need to recompile for each version
            echo "(would need recompile)"
        done
    done
done

echo ""
echo "To test a specific version:"
echo "1. Edit jni/version file with MAJOR, MINOR, PATCH values"
echo "2. Run build.bat to recompile"
echo "3. Push new binary to device"
echo "4. Test with: ./kpm_control $SUPERKEY get_status"
