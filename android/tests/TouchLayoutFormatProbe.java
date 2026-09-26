package com.dxxredux.app;

import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.zip.ZipFile;
import org.json.JSONObject;

// Runs with the installed APK and real Android KeyEvent codecs via app_process
public final class TouchLayoutFormatProbe {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static TouchLayout parse(JSONObject json) {
        HumanReadableConfig.ParseResult<TouchLayout> parsed =
                HumanReadableConfig.INSTANCE.humanJsonToTouchLayout(json);
        check(parsed.getWarnings().isEmpty(), parsed.getWarnings().toString());
        check(parsed.getValue() != null, "Touch layout parse failed");
        return parsed.getValue();
    }

    private static void roundTrip(TouchLayout layout) {
        check(layout.getVersion() == TouchLayoutRepositoryKt.CURRENT_TOUCH_LAYOUT_VERSION, "Unexpected schema");
        check(layout.equals(TouchLayout.Companion.fromJson(layout.toJson())), "Internal codec changed layout");
        check(layout.equals(parse(HumanReadableConfig.INSTANCE.touchLayoutToHumanJson(layout))),
                "Human-readable codec changed layout");
        ConfigSlotSet<TouchLayout> slots = new ConfigSlotSet<>(0,
                Collections.singletonList(new ConfigSlot<>("default", layout)));
        check(slots.equals(TouchLayoutSlotRepository.INSTANCE.fromExportJsonArray$app(
                TouchLayoutSlotRepository.INSTANCE.toExportJsonArray$app(slots), 0)), "Slot export changed layout");
    }

    public static void main(String[] args) throws Exception {
        check(new TouchLayout().getVersion() == TouchLayoutRepositoryKt.CURRENT_TOUCH_LAYOUT_VERSION,
                "Installed APK still creates an obsolete layout version");
        try (ZipFile apk = new ZipFile(args[0])) {
            for (String name : new String[] {"touch_default", "touch_controller_menus"}) {
                StringBuilder text = new StringBuilder();
                try (InputStreamReader reader = new InputStreamReader(
                        apk.getInputStream(apk.getEntry("assets/configs/touch/" + name + ".json")),
                        StandardCharsets.UTF_8)) {
                    char[] buffer = new char[4096];
                    int count;
                    while ((count = reader.read(buffer)) >= 0) text.append(buffer, 0, count);
                }
                TouchLayout layout = parse(new JSONObject(text.toString()));
                roundTrip(layout);
                check(Math.abs(layout.getGyro().getDeadzoneX() - 0.2293578f) < 0.000001f, "Yaw deadzone changed");
                check(Math.abs(layout.getGyro().getDeadzoneY() - 0.2293578f) < 0.000001f, "Roll deadzone changed");
                check(layout.getGyro().getDeadzoneZ() == 0.6f, "Pitch deadzone changed");
                if (name.equals("touch_default")) {
                    RadialMenuControl guide = null;
                    for (RadialMenuControl radial : layout.getRadialMenus()) {
                        if (radial.getId().equals("Guide")) guide = radial;
                    }
                    check(guide != null && guide.getCenterBinding() == -1, "Guide wheel changed");
                    List<Integer> bindings = new ArrayList<>();
                    for (RadialSegment segment : guide.getSegments()) bindings.add(segment.getBinding());
                    check(bindings.contains(TouchBindings.META_GUIDE_NEXT_GOAL)
                            && bindings.contains(TouchBindings.META_GUIDE_WARP_TO_ME)
                            && bindings.contains(TouchBindings.META_GUIDE_FIND_UNEXPLORED)
                            && bindings.contains(TouchBindings.META_GUIDE_FIND_SECRET)
                            && !bindings.contains(TouchBindings.META_GUIDE_RELEASE_CONTROL), "Guide actions changed");
                }
                System.out.println(name + ": internal, readable, and slot round trips passed");
            }
        }
    }
}
