package com.libertyrecomp;

public final class G925XDeviceProfileTest {
    public static void main(String[] args) {
        check(G925XDeviceProfile.matchesTarget("Xiaomi", "POCO X8 Pro Max", "Dimensity 9500s"), "marketing model");
        check(G925XDeviceProfile.matchesTarget("Xiaomi", "2602BPC18G", ""), "device code without SOC_MODEL");
        check(G925XDeviceProfile.matchesTarget("POCO", "poco x8 pro max", ""), "POCO brand");
        check(!G925XDeviceProfile.matchesTarget("Xiaomi", "POCO X8 Pro", "Dimensity 9500s"), "different phone");
        check(!G925XDeviceProfile.matchesTarget("Samsung", "Galaxy S26", "Dimensity 9500s"), "different vendor");
        check(!G925XDeviceProfile.matchesTarget(null, null, null), "null telemetry");
        check(G925XDeviceProfile.describe("Xiaomi","2602BPC18G","").contains("detected"), "target label");
        check(G925XDeviceProfile.describe("Other","Unknown","Dimensity 9500s").contains("not verified"), "SOC fallback");
        check(G925XDeviceProfile.describe("Other","Unknown","Unknown").contains("not hardware-validated"), "unknown hardware");
        System.out.println("PASS: G925 X device profile matching / safe fallback");
    }
    private static void check(boolean ok, String name) {
        if (!ok) throw new AssertionError(name);
    }
}
