// A file manifest avoids native command-line limits on Windows
import fs from "node:fs/promises";
import path from "node:path";
import prettier from "prettier";
import xml from "@prettier/plugin-xml";

const [manifestPath, mode] = process.argv.slice(2);
const files = JSON.parse(await fs.readFile(manifestPath, "utf8"));
let failures = 0;
let changed = 0;
for (const file of files) {
    try {
        const source = await fs.readFile(file, "utf8");
        const extension = path.extname(file).toLowerCase();
        const parser = {
            ".json": "json",
            ".jsonc": "jsonc",
            ".yml": "yaml",
            ".yaml": "yaml",
            ".md": "markdown",
            ".xml": "xml",
            ".svg": "xml",
            ".html": "html",
            ".js": "babel",
            ".mjs": "babel",
            ".ts": "typescript",
            ".css": "css",
        }[extension];
        const formatted = await prettier.format(source, {
            filepath: file,
            parser,
            plugins: [xml],
            tabWidth: ["yaml", "markdown"].includes(parser) ? 2 : 4,
            printWidth: 120,
            proseWrap: "preserve",
            embeddedLanguageFormatting: "off",
            xmlWhitespaceSensitivity: "strict",
            htmlWhitespaceSensitivity: "strict",
            endOfLine: "lf",
        });
        if (source !== formatted) {
            changed++;
            console.log(`${mode === "fix" ? "Formatted" : "Needs formatting"}: ${file}`);
            if (mode === "fix") await fs.writeFile(file, formatted, "utf8");
        }
    } catch (error) {
        failures++;
        console.error(`${file}: ${error.message}`);
    }
}
console.log(`Prettier: ${files.length} files, ${changed} changes, ${failures} errors`);
process.exitCode = failures || (mode !== "fix" && changed) ? 1 : 0;
