"""Last-chance keys from the player's Act 3 map."""
RUBY, EMERALD, SAPPHIRE = 1, 2, 4
REST, TREASURE, ELITE = 4, 5, 1
LAST_MAP_ROW = 14
MAP_WIDTH = 7


def key_paths(nodes):
    """Map each coordinate to (keys on this tile, attainable key sets after it)."""
    result = {}
    for x, y, room, edges, burning in sorted(nodes, key=lambda row: row[1], reverse=True):
        keys = RUBY if room == REST else SAPPHIRE if room == TREASURE else EMERALD if room == ELITE and burning else 0
        after = {0} if y == LAST_MAP_ROW else {
            child_keys | later
            for dest in range(MAP_WIDTH) if edges & (1 << dest)
            for child_keys, futures in (result[dest, y + 1],)
            for later in futures
        }
        result[x, y] = keys, tuple(sorted(after))
    return result


def keeps_all_keys(paths, node, missing):
    keys, futures = paths[node]
    return any(((keys | later) & missing) == missing for later in futures)


def blocked_map_actions(paths, actions, missing):
    """Consider only legal map branches and leave them alone if every branch loses a key."""
    good = {action for action, node in actions if keeps_all_keys(paths, node, missing)}
    return [action for action, _ in actions if action not in good] if good else []


def last_key_opportunity(paths, node, key):
    return not any(later & key for later in paths[node][1])
