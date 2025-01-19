function install_windows(extension: Extension) {
    const system = window.SupercellSWF.system();

    const package_path = window.SupercellSWF.cwd + extension.path;
    const archiver_bin = window.SupercellSWF.cwd + "core/bin/windows/7z.exe";
    const unpack_log = window.SupercellSWF.cwd + "unpack_log.txt";

    const extensions_folder = FLfile.platformPathToURI(system.install_path) + "extensions/";
    const destination_folder = extensions_folder + extension.install;
    if (FLfile.exists(destination_folder))
    {
        FLfile.remove(destination_folder);
    }

    FLfile.createFolder(destination_folder);
    const command = `call "${FLfile.uriToPlatformPath(archiver_bin)}" x -y "${FLfile.uriToPlatformPath(package_path)}" -o"${FLfile.uriToPlatformPath(destination_folder)}" >> "${FLfile.uriToPlatformPath(unpack_log)}"`;
    let status = FLfile.runCommandLine(command);
    if (!status)
    {
        fl.trace(`Failed to unpack ${extension.path}`);
    }
}

(
    function () {
        const [os, version] = fl.version.split(" ");
        const system = window.SupercellSWF.system();

        let installed = [];
        for (const extension of window.SupercellSWF.manifest.extensions) {
            if (extension.type !== "extension") continue;
            if (installed.indexOf(extension.name) != -1) continue;
            let valid = true;
            if (extension.condition)
            {
                let conditions = extension.condition.split(",");
                for (let condition of conditions)
                {
                    const [prop, value] = condition.split(":");

                    if (prop == "cpuf")
                    {
                        if (system.cpu_features.indexOf(value) == -1)
                        {
                            valid = false;
                            break;
                        }
                    }
                }
            }
            if (!valid) continue;

            if (extension.variant_name)
            {
                fl.trace(`Installed "${extension.name}" extension with "${extension.variant_name}" variant`)
            }

            switch (os) {
                case "WIN":
                    install_windows(extension);
                    break;

                default:
                    throw Error("Unknown platform")
            }

            installed.push(extension.name);
        }
    }
)()