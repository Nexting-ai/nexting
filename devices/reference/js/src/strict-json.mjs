const encoder = new TextEncoder();

function scanString(text, start) {
  for (let index = start + 1; index < text.length; index += 1) {
    if (text[index] === '"') return index + 1;
    if (text[index] === "\\") index += text[index + 1] === "u" ? 5 : 1;
  }
  return null;
}

export function hasUniqueObjectKeys(text, maximumBytes) {
  if (
    typeof text !== "string" ||
    (maximumBytes !== undefined && encoder.encode(text).length > maximumBytes)
  ) {
    return false;
  }
  const stack = [];
  for (let index = 0; index < text.length;) {
    const value = text[index];
    if (value === '"') {
      const end = scanString(text, index);
      if (end === null) return false;
      const current = stack.at(-1);
      if (current?.type === "object" && current.expectingKey) {
        let key;
        try {
          key = JSON.parse(text.slice(index, end));
        } catch {
          return false;
        }
        if (current.keys.has(key)) return false;
        current.keys.add(key);
        current.expectingKey = false;
      }
      index = end;
      continue;
    }
    if (value === "{") {
      stack.push({ type: "object", keys: new Set(), expectingKey: true });
    } else if (value === "[") {
      stack.push({ type: "array" });
    } else if (value === "}" || value === "]") {
      stack.pop();
    } else if (value === ",") {
      const current = stack.at(-1);
      if (current?.type === "object") current.expectingKey = true;
    }
    index += 1;
  }
  return stack.length === 0;
}
