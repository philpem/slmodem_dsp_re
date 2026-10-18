"""Strict physical mutation targets with proof of their logical function owner."""
from contextlib import contextmanager
from functools import lru_cache
from pathlib import Path
import re


class SourceError(ValueError):
    pass


def _masked(text):
    # Preserve offsets while hiding comments and strings from brace/paren walks.
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: re.sub(r'[^\n]', ' ', m.group()), text, flags=re.S)


def _close(text, start, left, right):
    depth = 0
    for i in range(start, len(text)):
        depth += (text[i] == left) - (text[i] == right)
        if depth == 0:
            return i
    raise SourceError('unclosed function declaration or body')


def function_spans(text):
    """Real column-zero definitions, bounded by balanced bodies, not next names."""
    mask = _masked(text)
    for match in re.finditer(r'^([A-Za-z_]\w*(?:::[A-Za-z_~]\w*)*)\(', mask, re.M):
        end = _close(mask, match.end() - 1, '(', ')') + 1
        while end < len(mask) and mask[end].isspace():
            end += 1
        if end < len(mask) and mask[end] == '{':
            yield match.group(1).split('::')[-1], end, _close(mask, end, '{', '}')


@lru_cache(maxsize=4)
def _proof_index(text):
    # Key by content so edits invalidate ownership without filesystem caches.
    mask = _masked(text)
    includes = []
    guard = re.match(r'\s*#\s*ifndef\s+(\w+)\s*\n\s*#\s*define\s+\1\b', mask)
    directives = list(re.finditer(
        r'^[ \t]*#\s*(if|ifdef|ifndef|elif|else|endif)\b[^\n]*', mask, re.M))
    events = [(m.start(), 'condition', m) for m in directives]
    events += [(m.start(), 'include', m) for m in re.finditer(
        r'^[ \t]*#\s*include\s+"([^"\n]+)"', text, re.M)]
    stack = []
    for _, kind, event in sorted(events, key=lambda item: item[0]):
        sharp = text.index('#', event.start(), event.end())
        if mask[sharp] != '#':
            continue
        if kind == 'condition':
            directive = event.group(1)
            if directive in ('if', 'ifdef', 'ifndef'):
                # Only the conventional leading header guard is exempt.
                stack.append(bool(guard and not stack and
                                  sharp == mask.index('#')))
            elif directive in ('else', 'elif'):
                if stack:
                    stack[-1] = False
            elif stack:
                stack.pop()
        elif not stack or (len(stack) == 1 and stack[0]):
            includes.append((sharp, event.group(1)))
    return tuple(function_spans(text)), tuple(includes)


def resolve_source(default, mutation, root=Path('.'), *, validate_anchor=True):
    """Return (physical path, proven logical owner or None) for one row.

    Existing single-file rows retain their original validation in callers.
    Overrides require an unconditional literal include in the declared suite
    function (a conventional leading header guard is allowed). Supported quoted
    include search is the facade directory followed by the repository include/
    directory; an adjacent file takes precedence. No preprocessor configuration
    or arbitrary conditional branch is inferred. Live-mutant inspection may
    disable anchor-presence validation; ownership and path proof remain strict.
    """
    if 'source' not in mutation:
        return Path(default), None
    root = Path(root).resolve()
    if not isinstance(mutation['source'], str) or not mutation['source']:
        raise SourceError('mutation source must be a nonempty path string')
    relative = Path(mutation['source'])
    if relative.is_absolute() or '..' in relative.parts:
        raise SourceError('mutation source must be a repository-relative path without ..')
    target = (root / relative).resolve()
    if not target.is_relative_to(root) or not target.is_file():
        raise SourceError('mutation source missing or outside repository')
    owner = mutation.get('fn')
    if not isinstance(owner, str) or not owner:
        raise SourceError('source override requires explicit fn')
    facade = (root / default).resolve()
    text = facade.read_text()
    definitions, includes = _proof_index(text)
    spans = [(a, z) for name, a, z in definitions if name == owner]
    if len(spans) != 1:
        raise SourceError('override fn must name exactly one suite-source definition')
    start, end = spans[0]
    # The mask proves that the # itself is outside a comment/string; quoted
    # include text is read from the original source, never inferred from a name.
    proofs = []
    for sharp, include in includes:
        if start < sharp < end:
            paths = (facade.parent / include, root / 'include' / include)
            winner = next((p.resolve() for p in paths if p.is_file()), None)
            if winner == target:
                proofs.append(sharp)
    if len(proofs) != 1:
        raise SourceError('override target needs one unconditional literal include inside fn body')
    content = target.read_text()
    if validate_anchor and content.count(mutation['find']) != 1:
        raise SourceError('override anchor must match exactly once')
    if mutation['find'] == mutation['replace']:
        raise SourceError('override replacement is vacuous')
    return relative, owner


@contextmanager
def mutated_file(path, original, replacement):
    """Restore this row's target even when building or classifying raises."""
    try:
        Path(path).write_text(replacement)
        yield
    finally:
        Path(path).write_text(original)
