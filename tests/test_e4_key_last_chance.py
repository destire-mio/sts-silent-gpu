from train.e4_key_last_chance import (
    EMERALD, RUBY, SAPPHIRE, blocked_map_actions, key_paths, last_key_opportunity,
)


def test_last_chance_keys_follow_edges_and_ignore_other_paths():
    nodes = [
        (0, 12, 4, 0b001, False),
        (1, 12, 5, 0b010, False),
        (2, 12, 5, 0b100, False),
        (0, 13, 1, 0b001, True),
        (1, 13, 0, 0b010, False),
        (2, 13, 5, 0b100, False),
        (0, 14, 4, 0b1000, False),
        (1, 14, 4, 0b1000, False),
        (2, 14, 4, 0b1000, False),
    ]
    paths = key_paths(nodes)
    assert not last_key_opportunity(paths, (0, 12), RUBY)
    assert last_key_opportunity(paths, (1, 12), SAPPHIRE)
    assert not last_key_opportunity(paths, (2, 12), SAPPHIRE)
    assert last_key_opportunity(paths, (0, 14), RUBY)
    assert blocked_map_actions(paths, [(4, (0, 12)), (5, (1, 12))], EMERALD | RUBY) == [5]
    assert blocked_map_actions(paths, [(4, (0, 12)), (5, (1, 12))], EMERALD | RUBY | SAPPHIRE) == []


def test_map_preserves_all_missing_keys_on_the_same_path():
    nodes = [
        (0, 13, 0, 0b011, False),
        (1, 13, 4, 0b010, False),
        (0, 14, 4, 0b1000, False),
        (1, 14, 1, 0b1000, True),
    ]
    paths = key_paths(nodes)
    assert blocked_map_actions(paths, [(4, (0, 13)), (5, (1, 13))], RUBY | EMERALD) == [4]
    assert blocked_map_actions(paths, [(4, (0, 13)), (5, (1, 13))], EMERALD) == []
