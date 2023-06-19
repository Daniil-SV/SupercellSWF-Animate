import Locale from "../../../Localization";
import { State, TextureDimensions, TextureScaleFactor } from "../../publisherState";

import BoolField from "../../Shared/BoolField";
import SubMenu from "../../Shared/SubMenu";
import EnumField from "../../Shared/EnumField";

export default function TextureSettings() {
    const exportToExternal = BoolField(
        Locale.Get("TID_SWF_SETTINGS_HAS_TEXTURE"),
        State.getParam("hasTexture"),
        {
            marginBottom: "6px"
        },
        value => (State.setParam("hasTexture", value)),
    );

    const scaleFactor = EnumField(
        Locale.Get("TID_SWF_SETTINGS_SCALE_FACTOR"),
        "sprite_scale_factor_select",
        TextureScaleFactor,
        State.getParam("textureScaleFactor"),
        {
            marginBottom: "6px"
        },
        value => (State.setParam("textureScaleFactor", value)),
    );

    const textureWidth = EnumField(
        Locale.Get("TID_SWF_SETTINGS_MAX_TEXTURE_WIDTH"),
        "texture_width_select",
        TextureDimensions,
        "2",
        {
            marginBottom: "6px"
        },
        value => (State.setParam("textureMaxWidth", TextureDimensions[value as any])),
    )

    const textureHeight = EnumField(
        Locale.Get("TID_SWF_SETTINGS_MAX_TEXTURE_HEIGHT"),
        "texture_height_select",
        TextureDimensions,
        "2",
        {
            //marginBottom: "6px"
        },
        value => (State.setParam("textureMaxHeight", TextureDimensions[value as any])),
    )

    return SubMenu(
        Locale.Get("TID_TEXTURES_LABEL"),
        "texture_settings",
        {
            marginBottom: "6px"
        },
        exportToExternal,
        scaleFactor,
        textureWidth,
        textureHeight
    )
}