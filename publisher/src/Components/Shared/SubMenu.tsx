import { CSSProperties, ReactNode, createElement } from "react";
import { AppTheme, AppThemes } from "../themes";
import { useState } from "react";
import TextField from "./TextField";

export default function SubMenu(
    name: string,
    keyName: string,
    style: CSSProperties = {},
    ...items: ReactNode[]
) {
    const [active, setActive] = useState(false);

    const delim = <hr key={`menu_${keyName}_delim`} style={{
        width: "50%",
        marginRight: "50%",
        marginTop: "5px",
        border: "2px solid #484848",
    }} />;

    const storage = createElement(
        "div",
        {
            key: `menu_${keyName}_storage`,
            style: {
                paddingLeft: "5%"
            }
        },
        delim,
        ...items
    );

    const button = createElement(
        "input",
        {
            type: "image",
            src: require(`../../images/${AppThemes[AppTheme]}/arrow.png`),
            style: {
                transform: active ? "rotate(180deg)" : "",
                transition: 'transform 100ms ease-in-out',
                width: "15px",
                height: "10px",
                verticalAlign: "middle"
            },
            onClick: function () {
                setActive(active === true ? false : true);
            }
        }
    );

    const label = TextField(
        name,
        {
            color: "white",
            paddingLeft: "10px",
            fontSize: "18px"
        }
    )

    if (active) {

        return createElement(
            "div",
            {
                key: `menu_${keyName}`,
                style: style
            },
            button,
            label,
            storage
        );

    } else {

        return createElement(
            "div",
            {
                key: `menu_${keyName}`,
                style: style
            },
            button,
            label
        );

    }


}