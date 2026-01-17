file(READ "${INPUT_FILE}" content)

file(WRITE "${OUTPUT_FILE}"
    "#pragma once\n\n"
    "namespace oblo::gen\n"
    "{\n"
    "    constexpr const char ${NAME}[] = R\"(${content})\";\n\n"
    "}\n"
)
