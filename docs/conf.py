project   = "@PROJECT_NAME@"
release   = "@PROJECT_VERSION@"
author = ""

extensions = [
    "myst_parser",
]

root_doc = "index"

myst_enable_extensions = [
    "deflist",       # definition lists  (dl/dt/dd) — useful for CLI options, config keys
    "colon_fence",   # ::: as directive shorthand   — cleaner admonitions
]

html_theme = "sphinx_rtd_theme"
html_title = f"{project} {release}"

man_pages = [
    ("usage",  "openpressod",      f"{project} daemon",            [author], 8),
    ("config", "@OPENPRESSOD_CONFIG_NAME@", f"{project} configuration file",[author], 5),
]

latex_documents = [
    # (root_doc, output .tex file, title, author, documentclass)
    ("index", f"{project}.tex", f"{project} Manual", author, "manual"),
]

latex_elements = {
    "papersize": "a4paper",
    "pointsize": "11pt",
}