import pytest

from train.e4_learner import update_beta


def test_imitation_beta_update_and_clamp():
    assert update_beta(.1, .7, .4) == pytest.approx(.13)
    assert update_beta(.1, .4, .7) == pytest.approx(.07)
    assert update_beta(0., .7, .7) == 0.
    assert update_beta(0., .4, .7) == 0.
    assert update_beta(.49, .7, .1) == .5
    assert update_beta(.01, .1, .7) == 0.
