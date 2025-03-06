import numpy as np
from scipy.signal import convolve

styles = [
    {
        "color": "#a00000",
        "linestyle": "-",
        "marker": "o",
        "markevery": 1,
        "markersize": 5,
        # "markerfacecolor": "None",
    },
    {
        "color": "#00a000",
        "linestyle": "-",
        "marker": "x",
        "markersize": 15,
        "markevery": 1,
        "markerfacecolor": "None",
    },
    {
        "color": "#5060d0",
        "linestyle": "-",
        "marker": ">",
        "markersize": 10,
        "markevery": 1,
    },
    {
        "color": "#f25900",
        "linestyle": "-",
        "marker": "+",
        "markevery": 1,
        "markersize": 15,
        # 'markerfacecolor': 'None',
    },
    {
        "color": "#500050",
        "linestyle": "-",
        "markevery": 30,
        "markersize": 20,
    },
]

scatter_styles = [
    {"color": "#a00000", "marker": "o"},
    {
        "color": "#00a000",
        "marker": "x",
    },
    {
        "color": "#5060d0",
        "marker": ">",
    },
    {
        "color": "#f25900",
        "marker": "+",
    },
]


class BaseDraw:
    def __init__(self, exp_name, style, font_size):
        self.__exp_name = exp_name
        self.__style = style
        self.__font_size = font_size

    def action(self):
        raise NotImplementedError("Subclass must implement this abstract method")

    def convolve_result(self, window_size, x, y):
        kernel = np.ones(window_size) / window_size
        y_conv = convolve(y, kernel, mode="valid")
        left_padding_size = (window_size - 1) // 2
        right_padding_size = (
            len(x)
            - left_padding_size
            - (1 if left_padding_size * 2 < (window_size - 1) else 0)
        )
        return x[left_padding_size:right_padding_size], y_conv
