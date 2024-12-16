import Locale from "../../../Localization";
import BoolField from "../../Shared/BoolField";
import EnumField from "../../Shared/EnumField";
import SubMenu from "../../Shared/SubMenu";
import { BaseCompressionMethods, CompressionMethods, Settings, SWFType } from "../../../PublisherSettings";
import { GetPublishContext } from "../../../Context";
import { ReactNode } from "react";

export default function OtherSettings() {
    const { fileType, useBackwardCompatibility } = GetPublishContext();

    const compressionType = new EnumField({
        name: Locale.Get("TID_SWF_SETTINGS_COMPRESSION"),
        keyName: "file_compression_select",
        enumeration: useBackwardCompatibility ? BaseCompressionMethods : CompressionMethods,
        defaultValue: Settings.getParam("compressionMethod"),
        style: {
            display: "flex",
            alignItems: "center",
            marginBottom: "10px"
        },
        callback: value => (Settings.setParam("compressionMethod", parseInt(value))),
    })

    const precisionMatrix = new BoolField(
        {
            name: Locale.Get("TID_SWF_SETTINGS_PRECISION_MATRIX"),
            keyName: "precision_matrix",
            defaultValue: Settings.getParam("hasPrecisionMatrices"),
            style: {
                display: "flex",
                alignItems: "center",
                marginBottom: "10px"
            },
            callback: value => (Settings.setParam("hasPrecisionMatrices", value)),
        }
    );

    const writeCustomProperties = new BoolField(
        {
            name: Locale.Get("TID_SWF_WRITE_CUSTOM_PROPERTIES"),
            keyName: "custom_properties",
            defaultValue: Settings.getParam("writeCustomProperties"),
            style: {
                display: "flex",
                alignItems: "center",
                marginBottom: "10px"
            },
            callback: value => (Settings.setParam("writeCustomProperties", value)),
        }
    );

    if (useBackwardCompatibility)
    {
        Settings.setParam("hasPrecisionMatrices", false);
        precisionMatrix.state.checked = false;

        Settings.setParam("writeCustomProperties", false);
        writeCustomProperties.state.checked = false;

        Settings.setParam("compressionMethod", CompressionMethods.LZMA);
    }

    let sc1_dependent_options: ReactNode[] = [];
    if (fileType == SWFType.SC1)
    {
        sc1_dependent_options = [
            compressionType.render(),
            !useBackwardCompatibility ? writeCustomProperties.render() : undefined,
            !useBackwardCompatibility ? precisionMatrix.render() : undefined
        ]
    }

    return SubMenu(
        Locale.Get("TID_OTHER_LABEL"),
        "other_settings",
        {
            marginBottom: "20%"
        },
        ...sc1_dependent_options
    )
}