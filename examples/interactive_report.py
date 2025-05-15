import os
import json
import pandas as pd
import dash
from dash import dcc, html, dash_table, Input, Output, State, ctx, MATCH, ALL, ALLSMALLER, callback_context
import plotly.express as px
import shutil
import datetime
import numpy as np
from dash.exceptions import PreventUpdate

DEFAULT_RESULTS_PATH = os.path.join(os.path.dirname(__file__), 'output/performance_results.json')

def load_results(path):
    if not os.path.exists(path):
        return None, f"Results file not found: {path}"
    try:
        with open(path) as f:
            data = json.load(f)
        results = data.get('benchmark_results', [])
        if not results:
            return None, "No benchmark_results found in JSON."
        df = pd.json_normalize(results)
        return df, None
    except Exception as e:
        return None, f"Error loading file: {e}"

# Initial load
init_df, error_msg = load_results(DEFAULT_RESULTS_PATH)
example_options = [{'label': ex, 'value': ex} for ex in sorted(init_df['example_name'].unique())] if init_df is not None else []
thread_options = [{'label': str(t), 'value': t} for t in sorted(init_df['threads'].unique())] if init_df is not None else []
correctness_options = [{'label': c, 'value': c} for c in ['ALL', 'CARTS_ONLY', 'OMP_ONLY', 'BOTH', 'EITHER']]

# --- Styling ---
MAIN_BG_COLOR = '#e6f0fa'  # pleasant light blue
CARD_BG_COLOR = '#ffffff'  # white for cards
PRIMARY_COLOR = '#1f77b4'  # blue (matches plot color)
ACCENT_COLOR = '#ff7f0e'   # orange (matches plot color)

main_div_style = {
    'background': 'linear-gradient(135deg, #e6f0fa 0%, #f7fbff 100%)',
    'minHeight': '100vh',
    'padding': '0',
    'margin': '0',
    'fontFamily': 'Segoe UI, Arial, sans-serif',
}
card_style = {
    'backgroundColor': CARD_BG_COLOR,
    'borderRadius': '12px',
    'boxShadow': '0 4px 16px rgba(31,119,180,0.10)',
    'padding': '32px 32px 24px 32px',
    'margin': '32px auto',
    'maxWidth': '1400px',
}
header_style = {
    'color': PRIMARY_COLOR,
    'fontWeight': 'bold',
    'fontSize': '2.5rem',
    'marginBottom': '8px',
    'textAlign': 'center',
    'letterSpacing': '1px',
    'background': 'linear-gradient(90deg, #e6f0fa 60%, #d0e3f7 100%)',
    'borderRadius': '12px',
    'boxShadow': '0 2px 8px rgba(31,119,180,0.07)',
    'padding': '18px 0 12px 0',
}
subtitle_style = {
    'color': '#444',
    'fontSize': '1.2rem',
    'textAlign': 'center',
    'marginBottom': '24px',
}
label_style = {
    'fontWeight': 'bold',
    'color': PRIMARY_COLOR,
    'marginTop': '12px',
    'fontSize': '1.08rem',
}
# Use a single dropdown_style for all filter dropdowns (input box only)
dropdown_style = {
    'marginBottom': '0',
    'backgroundColor': '#f7fbff',
    'borderRadius': '8px',
    'zIndex': 2000,
    'width': '240px',
    'fontSize': '1.08rem',
    'padding': '8px',
    'boxShadow': '0 2px 8px rgba(31,119,180,0.07)',
    'minHeight': '44px',
    'maxHeight': '44px',
}
# For multi-select, fixed height, horizontal scroll for chips, no vertical scroll, and max width to prevent overlap
multi_dropdown_style = {**dropdown_style, 'height': '44px', 'overflowX': 'auto', 'whiteSpace': 'nowrap', 'maxWidth': '220px', 'minWidth': '180px'}

filter_box_style = {
    'width': '240px',
    'minHeight': '70px',
    'display': 'flex',
    'flexDirection': 'column',
    'alignItems': 'center',
    'justifyContent': 'center',
    'marginRight': '12px',
    'marginBottom': '8px',
    'background': 'none',
}

footer_style = {
    'textAlign': 'center',
    'color': '#888',
    'marginTop': '32px',
    'fontSize': '1rem',
    'paddingBottom': '16px',
}
summary_card_style = {
    'display': 'inline-block',
    'backgroundColor': '#f7fbff',
    'borderRadius': '10px',
    'boxShadow': '0 2px 8px rgba(31,119,180,0.07)',
    'padding': '10px 12px',
    'margin': '0 8px 8px 0',
    'minWidth': '120px',
    'textAlign': 'center',
    'fontSize': '1.02rem',
    'verticalAlign': 'top',
    'transition': 'box-shadow 0.2s, transform 0.2s',
    'cursor': 'pointer',
}
summary_card_hover_style = {
    'boxShadow': '0 6px 18px rgba(31,119,180,0.18)',
    'transform': 'translateY(-2px) scale(1.03)',
}
button_style = {
    'backgroundColor': PRIMARY_COLOR,
    'color': 'white',
    'border': 'none',
    'borderRadius': '6px',
    'padding': '6px 18px',
    'fontWeight': 'bold',
    'transition': 'box-shadow 0.2s, background 0.2s',
    'boxShadow': '0 2px 8px rgba(31,119,180,0.07)',
    'cursor': 'pointer',
}
button_hover_style = {
    'backgroundColor': ACCENT_COLOR,
    'boxShadow': '0 4px 12px rgba(255,127,14,0.13)',
}

# Stylish dropdown style for blue background and scrollable menu
stylish_dropdown_style = {**dropdown_style, 'backgroundColor': '#e6f0fa', 'borderRadius': '8px', 'minWidth': '160px', 'maxWidth': '320px', 'zIndex': 1000, 'fontSize': '1.08rem', 'padding': '8px', 'flexGrow': 1}
stylish_menu_style = {'backgroundColor': '#f7fbff', 'maxHeight': '220px', 'overflowY': 'auto', 'borderRadius': '8px'}

# Use this style for all help text containers under the tabs
help_text_style = {
    'textAlign': 'left',
    'margin': '8px 0 12px 0',
    'display': 'flex',
    'alignItems': 'center',
    'minHeight': '32px',
    'maxHeight': '32px',
    'overflow': 'hidden',
}

# Modal style
modal_overlay_style = {
    'position': 'fixed', 'top': 0, 'left': 0, 'width': '100vw', 'height': '100vh',
    'backgroundColor': 'rgba(0,0,0,0.35)', 'zIndex': 9999, 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'center'
}
filter_modal_overlay_style = {**modal_overlay_style, 'zIndex': 99999}
modal_content_style = {
    'backgroundColor': '#fff', 'borderRadius': '12px', 'boxShadow': '0 8px 32px rgba(31,119,180,0.18)',
    'padding': '28px 20px 20px 20px', 'maxWidth': '370px', 'minWidth': '240px', 'textAlign': 'left',
    'fontSize': '1.08rem',
}
filter_modal_content_style = modal_content_style
filter_modal_close_style = {
    'position': 'absolute', 'top': '12px', 'right': '18px', 'fontSize': '1.5em', 'color': ACCENT_COLOR, 'cursor': 'pointer', 'fontWeight': 'bold', 'border': 'none', 'background': 'none'
}

# Help text for each card
CARD_HELP = {
    'geomean': 'The geometric mean of all speedups (OMP/CARTS) across all filtered runs. A value >1 means OMP is faster on average, <1 means CARTS is faster.',
    'median': 'The median speedup (OMP/CARTS) across all filtered runs. Less sensitive to outliers than the geometric mean.',
    'correctness': 'The percentage of runs marked as CORRECT (for both OMP and CARTS).',
    'unique': 'The number of unique benchmark examples in the filtered data.',
    'slowdowns': 'The number of runs where OMP is slower than CARTS (speedup < 1).',
}

# --- Helper to render metadata/reproducibility info ---

def render_metadata(data, results_path):
    sysinfo = data.get('system_information', {}) if data else {}
    depvers = data.get('dependency_versions', {}) if data else {}
    repro = data.get('reproducibility', {}) if data else {}
    agg = data.get('aggregate_statistics', {}) if data else {}
    items = []
    if results_path:
        items.append(html.Div([html.B('Results file: '), html.Span(results_path)]))
    if depvers:
        items.append(html.Div([html.B('Dependency versions:')]))
        for k, v in depvers.items():
            items.append(html.Div([html.Span(f"- {k}: {v}")], style={'marginLeft': '16px'}))
    if repro:
        items.append(html.Div([html.B('Python: '), html.Span(repro.get('python_version', ''))]))
        items.append(html.Div([html.B('Git commit: '), html.Span(repro.get('git_commit', ''))]))
        if repro.get('env'):
            items.append(html.Div([html.B('Env vars:')]))
            for k, v in repro['env'].items():
                items.append(html.Div([html.Span(f"- {k}: {v}")], style={'marginLeft': '16px'}))
        if repro.get('pip_freeze'):
            items.append(html.Div([html.B('Python packages:')]))
            for pkg in repro['pip_freeze'][:10]:
                items.append(html.Div([html.Span(pkg)], style={'marginLeft': '16px'}))
            if len(repro['pip_freeze']) > 10:
                items.append(html.Div([html.Span('...')], style={'marginLeft': '16px'}))
    if agg:
        items.append(html.Div([html.B('Geomean speedup: '), html.Span(f"{agg.get('geometric_mean_speedup', 'N/A'):.2f}x" if agg.get('geometric_mean_speedup') else 'N/A')]))
        items.append(html.Div([html.B('CARTS wins: '), html.Span(str(agg.get('carts_wins', 'N/A')))]))
        items.append(html.Div([html.B('OMP wins: '), html.Span(str(agg.get('omp_wins', 'N/A')))]))
    # System info at the bottom
    if sysinfo:
        sysinfo_items = []
        if sysinfo.get('os_platform'):
            sysinfo_items.append(html.Div([html.B('System: '), html.Span(sysinfo.get('os_platform'))]))
        if sysinfo.get('cpu_model'):
            sysinfo_items.append(html.Div([html.B('CPU: '), html.Span(sysinfo.get('cpu_model'))]))
        if sysinfo.get('cpu_cores'):
            sysinfo_items.append(html.Div([html.B('Cores: '), html.Span(str(sysinfo.get('cpu_cores')))]))
        if sysinfo.get('memory_total_mb'):
            sysinfo_items.append(html.Div([html.B('RAM: '), html.Span(str(sysinfo.get('memory_total_mb')) + ' MB')]))
        if sysinfo.get('clang_version'):
            sysinfo_items.append(html.Div([html.B('Clang: '), html.Span(sysinfo.get('clang_version'))]))
        if sysinfo.get('timestamp'):
            sysinfo_items.append(html.Div([html.B('Timestamp: '), html.Span(sysinfo.get('timestamp'))]))
        items.append(html.Hr(style={'margin': '18px 0 10px 0', 'border': 'none', 'height': '1px', 'background': '#e6f0fa'}))
        items.append(html.Div([
            html.Span('System Information', style={'fontWeight': 'bold', 'color': ACCENT_COLOR, 'fontSize': '1.1rem', 'marginTop': '8px'}),
            html.Div(sysinfo_items, style={'marginTop': '10px', 'fontSize': '0.98rem', 'color': '#333'})
        ], style={'marginBottom': '0', 'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '8px 12px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)'}))
    if not items:
        items.append(html.I('No metadata available.'))
    return html.Details([
        html.Summary('Reproducibility & Metadata', style={'fontWeight': 'bold', 'color': ACCENT_COLOR, 'fontSize': '1.1rem', 'marginTop': '18px'}),
        html.Div(items, style={'marginTop': '10px', 'fontSize': '0.98rem', 'color': '#333'})
    ], open=False, style={'marginBottom': '18px', 'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '12px 18px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)'})

def render_summary_cards(meta):
    agg = meta.get('aggregate_statistics', {}) if meta else {}
    correctness = agg.get('correctness_counts', {}) if agg else {}
    geomean = agg.get('geometric_mean_speedup', None)
    median_speedup = agg.get('median_speedup', None)
    correctness_rate = agg.get('correctness_rate', None)
    slowdowns = agg.get('slowdowns', 0)
    unique_examples = agg.get('unique_examples', 0)
    # Card style tweaks (no icons)
    card_base = {'display': 'inline-block', 'backgroundColor': '#f7fbff', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'padding': '10px 12px', 'margin': '0 8px 8px 0', 'minWidth': '120px', 'textAlign': 'center', 'fontSize': '1.02rem', 'verticalAlign': 'top', 'transition': 'box-shadow 0.2s, transform 0.2s', 'cursor': 'pointer'}
    # Cards
    cards = []
    # Geomean speedup
    cards.append(html.Div([
        html.Div([
            'Geomean Speedup',
            html.Span(' ⓘ', id='help-geomean', n_clicks=0, style={'cursor': 'pointer', 'color': ACCENT_COLOR, 'fontSize': '1.1em', 'marginLeft': '4px'})
        ], style={'color': PRIMARY_COLOR, 'fontWeight': 'bold'}),
        html.Div(f"{geomean:.2f}x" if geomean else 'N/A', style={'fontSize': '1.2rem', 'fontWeight': 'bold'})
    ], style={**card_base, 'backgroundColor': '#e6f7e6' if geomean and geomean > 1 else '#fffbe6'}))
    # Median speedup
    cards.append(html.Div([
        html.Div([
            'Median Speedup',
            html.Span(' ⓘ', id='help-median', n_clicks=0, style={'cursor': 'pointer', 'color': ACCENT_COLOR, 'fontSize': '1.1em', 'marginLeft': '4px'})
        ], style={'color': PRIMARY_COLOR, 'fontWeight': 'bold'}),
        html.Div(f"{median_speedup:.2f}x" if median_speedup else 'N/A', style={'fontSize': '1.2rem', 'fontWeight': 'bold'})
    ], style=card_base))
    # Correctness rate
    cards.append(html.Div([
        html.Div([
            'Correctness Rate',
            html.Span(' ⓘ', id='help-correctness', n_clicks=0, style={'cursor': 'pointer', 'color': ACCENT_COLOR, 'fontSize': '1.1em', 'marginLeft': '4px'})
        ], style={'color': PRIMARY_COLOR, 'fontWeight': 'bold'}),
        html.Div(f"{correctness_rate:.1f}%" if correctness_rate is not None else 'N/A', style={'fontSize': '1.2rem', 'fontWeight': 'bold'}),
        html.Div([
            html.Div(style={
                'width': f'{correctness_rate if correctness_rate is not None else 0}%','height': '8px','backgroundColor': '#4caf50','borderRadius': '4px','margin': '4px 0'}),
            html.Div(style={
                'width': f'{100 - correctness_rate if correctness_rate is not None else 100}%','height': '8px','backgroundColor': '#f44336','borderRadius': '4px','margin': '4px 0','display': 'inline-block'})
        ], style={'width': '100%', 'backgroundColor': '#eee', 'borderRadius': '4px', 'overflow': 'hidden', 'height': '8px', 'margin': '4px 0'})
    ], style={**card_base, 'backgroundColor': '#e6f7e6'}))
    # Unique examples
    cards.append(html.Div([
        html.Div([
            'Unique Examples',
            html.Span(' ⓘ', id='help-unique', n_clicks=0, style={'cursor': 'pointer', 'color': ACCENT_COLOR, 'fontSize': '1.1em', 'marginLeft': '4px'})
        ], style={'color': PRIMARY_COLOR, 'fontWeight': 'bold'}),
        html.Div(str(unique_examples), style={'fontSize': '1.2rem', 'fontWeight': 'bold'})
    ], style=card_base))
    # Slowdown card
    if slowdowns:
        cards.append(html.Div([
            html.Div([
                'Runs with Slowdown',
                html.Span(' ⓘ', id='help-slowdowns', n_clicks=0, style={'cursor': 'pointer', 'color': ACCENT_COLOR, 'fontSize': '1.1em', 'marginLeft': '4px'})
            ], style={'color': ACCENT_COLOR, 'fontWeight': 'bold'}),
            html.Div(str(slowdowns), style={'fontSize': '1.2rem', 'fontWeight': 'bold'})
        ], style={**card_base, 'backgroundColor': '#fffbe6'}))
    summary_section = html.Div(cards, style={'marginBottom': '10px', 'display': 'flex', 'flexWrap': 'wrap', 'gap': '6px', 'backgroundColor': '#f4f8fc', 'borderRadius': '12px', 'padding': '8px 8px 4px 8px', 'justifyContent': 'center', 'margin': '0 auto'})
    return html.Div([summary_section])

# --- Layout ---
app = dash.Dash(__name__, suppress_callback_exceptions=True)
app.layout = html.Div([
    dcc.Store(id='modal-store', data={'open': False, 'card': None}),
    dcc.Store(id='filters-store', data={'examples': [], 'threads': [], 'correctness': 'ALL'}),
    dcc.Store(id='filter-modal-store'),
    html.Div([
        html.H1("CARTS Performance Report", style=header_style),
        html.Div("Explore, compare, and reproduce performance results for CARTS vs OpenMP.", style=subtitle_style),
        dcc.Store(id='results-store'),
        html.Div([
            html.Label(["Results file:", html.Span(" ⓘ", title="Path to the results JSON file (e.g., output/performance_results.json)", style={'cursor': 'help', 'color': ACCENT_COLOR})], style={**label_style, 'width': 'auto', 'textAlign': 'right', 'marginRight': '10px', 'marginBottom': '0', 'alignSelf': 'center', 'minWidth': '120px'}),
            dcc.Input(id='results-path', type='text', value=DEFAULT_RESULTS_PATH, style={'width': '60%', 'marginRight': '0', 'marginBottom': '0', 'textAlign': 'left', 'height': '36px', 'lineHeight': '36px', 'padding': '0 10px', 'borderRadius': '6px', 'border': '1.5px solid #d0e3f7', 'fontSize': '1.01rem', 'alignSelf': 'center'}),
        ], style={'marginBottom': '8px', 'marginTop': '8px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'center', 'gap': '0'}),
        html.Div(id='data-status-message', style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'marginBottom': '12px'}),
        html.Div([
            html.Button('Filter', id='open-filter-modal-btn', n_clicks=0, style={'backgroundColor': PRIMARY_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '6px', 'padding': '10px 32px', 'fontWeight': 'bold', 'fontSize': '1.1rem', 'margin': '0 auto', 'display': 'block'}),
        ], style={'display': 'flex', 'justifyContent': 'center', 'alignItems': 'center', 'marginBottom': '18px', 'marginTop': '8px'}),
        html.Div(id='filter-modal'),
        html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),
        html.Div(id='summary-cards'),
        html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),
        html.Div([
            dcc.Tabs([
                dcc.Tab(label='Speedup', children=[
                    html.Div([
                        html.Span('ⓘ', style={'color': ACCENT_COLOR, 'fontSize': '1.2em', 'marginRight': '6px'}),
                        html.Span('Shows OMP speedup compared to CARTS for each example and thread.', style={'color': '#555', 'fontSize': '1.01em'})
                    ], style=help_text_style),
                    dcc.Graph(id='speedup-bar-plot', style={'height': '600px'})
                ], style={'fontWeight': 'bold', 'color': PRIMARY_COLOR}),
                dcc.Tab(label='Scaling', children=[
                    html.Div([
                        html.Span('ⓘ', style={'color': ACCENT_COLOR, 'fontSize': '1.2em', 'marginRight': '6px'}),
                        html.Span('Average execution time for OMP and CARTS by thread count.', style={'color': '#555', 'fontSize': '1.01em'})
                    ], style=help_text_style),
                    dcc.Graph(id='scaling-plot', style={'height': '600px'})
                ], style={'fontWeight': 'bold', 'color': PRIMARY_COLOR}),
                dcc.Tab(label='Strong Scaling', children=[
                    html.Div([
                        html.Span('ⓘ', style={'color': ACCENT_COLOR, 'fontSize': '1.2em', 'marginRight': '6px'}),
                        html.Span('Strong scaling speedup for OMP and CARTS (vs 1 thread).', style={'color': '#555', 'fontSize': '1.01em'})
                    ], style=help_text_style),
                    dcc.Graph(id='speedup-plot', style={'height': '600px'})
                ], style={'fontWeight': 'bold', 'color': PRIMARY_COLOR}),
                dcc.Tab(label='Correctness by Example', children=[
                    html.Div([
                        html.Span('ⓘ', style={'color': ACCENT_COLOR, 'fontSize': '1.2em', 'marginRight': '6px'}),
                        html.Span('Number of correct runs for OMP and CARTS per example.', style={'color': '#555', 'fontSize': '1.01em'})
                    ], style=help_text_style),
                    dcc.Graph(id='correctness-bar-plot', style={'height': '600px'})
                ], style={'fontWeight': 'bold', 'color': PRIMARY_COLOR}),
                dcc.Tab(label='Performance Variability', children=[
                    html.Div([
                        html.Span('ⓘ', style={'color': ACCENT_COLOR, 'fontSize': '1.2em', 'marginRight': '6px'}),
                        html.Span('Spread of execution times for OMP and CARTS per example.', style={'color': '#555', 'fontSize': '1.01em'})
                    ], style=help_text_style),
                    dcc.Graph(id='variability-box-plot', style={'height': '600px'})
                ], style={'fontWeight': 'bold', 'color': PRIMARY_COLOR}),
            ], style={'backgroundColor': CARD_BG_COLOR, 'borderRadius': '8px', 'display': 'flex', 'alignItems': 'center'}),
            # Move log/linear toggle below the tabs, centered
            html.Div([
                dcc.RadioItems(
                    id='plot-scale-toggle',
                    options=[
                        {'label': 'Linear', 'value': 'linear'},
                        {'label': 'Log', 'value': 'log'}
                    ],
                    value='linear',
                    labelStyle={
                        'display': 'inline-block',
                        'marginRight': '12px',
                        'fontWeight': 'bold',
                        'fontSize': '1.04rem',
                        'padding': '7px 18px',
                        'borderRadius': '22px',
                        'background': '#f7fbff',
                        'border': '1.5px solid #d0e3f7',
                        'marginBottom': '0',
                        'verticalAlign': 'middle',
                        'transition': 'background 0.2s, color 0.2s, border 0.2s',
                        'cursor': 'pointer',
                        'minWidth': '90px',
                        'textAlign': 'center',
                        'color': PRIMARY_COLOR,
                    },
                    inputStyle={
                        'marginRight': '6px',
                        'accentColor': ACCENT_COLOR,
                    },
                    style={
                        'display': 'inline-block',
                        'background': '#f7fbff',
                        'borderRadius': '22px',
                        'boxShadow': '0 1px 6px rgba(31,119,180,0.07)',
                        'padding': '0',
                        'margin': '18px 0 18px 0',
                        'height': '40px',
                        'verticalAlign': 'middle',
                        'overflow': 'hidden',
                    },
                    persistence=True,
                    persistence_type='session',
                ),
            ], style={
                'width': '100%',
                'display': 'flex',
                'alignItems': 'center',
                'justifyContent': 'center',
                'margin': '0 0 18px 0',
            }),
        ]),
        html.H2("Results Table", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '24px', 'marginBottom': '12px'}),
        html.Div(id='download-section'),
        dash_table.DataTable(id='results-table', page_size=10, style_table={'overflowX': 'auto', 'border': '1px solid #d0e3f7', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)'}, style_cell={'backgroundColor': CARD_BG_COLOR, 'color': '#222', 'fontFamily': 'monospace', 'fontSize': '1rem'}, style_header={'backgroundColor': PRIMARY_COLOR, 'color': 'white', 'fontWeight': 'bold'}, sort_action='native'),
        html.Div(id='metadata-panel-card'),
        html.Div(id='system-info-card'),
        # Modal dialog (conditionally rendered)
        html.Div(id='summary-modal'),
    ], style=card_style),
    html.Div([
        html.Span("CARTS Benchmarking Report | "),
        html.A("GitHub", href="https://github.com/ARTS-Lab/carts", target="_blank", style={'color': PRIMARY_COLOR, 'textDecoration': 'none', 'fontWeight': 'bold'}),
        html.Span(" | For scientific reproducibility. "),
        html.Span("© 2024")
    ], style=footer_style)
], style=main_div_style)

@app.callback(
    Output('results-store', 'data'),
    [Input('results-path', 'value')]
)
def load_results_to_store(path):
    if not path:
        return None
    df, error = load_results(path)
    if error or df is None or df.empty:
        return None
    return df.to_dict('records')

@app.callback(
    Output('download-section', 'children'),
    [Input('results-path', 'value')]
)
def update_download_section(results_path):
    import base64
    # Ensure assets/ exists
    if not os.path.exists('assets'):
        os.makedirs('assets', exist_ok=True)
    csv_path = results_path.replace('.json', '.csv') if results_path else None
    csv_exists = os.path.exists(csv_path) if csv_path else False
    assets_csv_path = os.path.join('assets', 'performance_results.csv')
    last_updated = None
    if csv_exists:
        try:
            shutil.copy(csv_path, assets_csv_path)
            last_updated = datetime.datetime.fromtimestamp(os.path.getmtime(csv_path)).strftime('%Y-%m-%d %H:%M:%S')
        except Exception as e:
            csv_exists = False
            last_updated = f"Error copying CSV: {e}"
    csv_link = html.A([
        html.Span("⬇️", style={'marginRight': '6px'}),
        'Download CSV Results'
    ], href='/assets/performance_results.csv' if csv_exists else '#', download='performance_results.csv' if csv_exists else None, target='_blank', style={'marginRight': '18px', 'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'textDecoration': 'underline', 'fontSize': '1.08rem'}, title='Download the raw CSV results') if csv_exists else html.Span([
        html.Span("⚠️", style={'marginRight': '6px'}),
        'CSV not found'
    ], style={'color': '#aaa', 'fontWeight': 'bold'}, title='CSV file not found in output/')
    # PDF downloads
    pdf_links = []
    images = []
    pdfs = []
    if os.path.exists('output'):
        images = [f for f in os.listdir('output') if f.endswith(('.png', '.jpg', '.jpeg'))]
        pdfs = [f for f in os.listdir('output') if f.endswith('.pdf')]
    for pdf in pdfs:
        pdf_path = os.path.join('output', pdf) if not pdf.startswith('output') else pdf
        pdf_link_path = f'/assets/{os.path.basename(pdf)}'
        # Copy/symlink PDF to assets if not already there
        if os.path.exists(pdf_path) and not os.path.exists(os.path.join('assets', os.path.basename(pdf))):
            try:
                shutil.copy(pdf_path, os.path.join('assets', os.path.basename(pdf)))
            except Exception:
                pass
        pdf_links.append(html.A([
            html.Span("⬇️", style={'marginRight': '4px'}),
            os.path.basename(pdf)
        ], href=pdf_link_path, download=os.path.basename(pdf), target='_blank', style={'marginRight': '12px', 'color': ACCENT_COLOR, 'textDecoration': 'underline', 'fontSize': '1.08rem'}, title='Download PDF graph'))
    return html.Div([
        html.Label(["Downloads:", html.Span(" ⓘ", title="Download the raw CSV results or generated PDF graphs", style={'cursor': 'help', 'color': ACCENT_COLOR})], style=label_style),
        html.Div([csv_link]),
        html.Div(pdf_links, style={'marginTop': '6px'}),
        html.Div([html.Span('Last updated: '), html.Span(last_updated or 'N/A', style={'color': '#888'})], style={'fontSize': '0.98rem', 'marginTop': '6px'})
    ], style={'marginBottom': '18px', 'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '10px 18px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'border': '1px solid #d0e3f7'})

@app.callback(
    Output('metadata-panel-card', 'children'),
    [Input('results-path', 'value')]
)
def update_metadata_panel_card(results_path):
    df, error = load_results(results_path)
    if error or df is None or df.empty:
        return html.Div([html.B("Error loading metadata: " + error or "No data")])
    return html.Div([
        render_metadata(df.iloc[0].to_dict(), results_path)
    ])

@app.callback(
    Output('summary-cards', 'children'),
    [Input('results-table', 'data')]
)
def update_summary_cards_from_filtered(data):
    if not data:
        return html.Div([html.B("No data to summarize.")])
    import pandas as pd
    import numpy as np
    df = pd.DataFrame(data)
    # Only consider positive speedups for median and geomean
    speedups = [s for s in df.get('speedup_carts_vs_omp', []) if pd.notnull(s) and s > 0]
    geomean = np.exp(np.mean(np.log(speedups))) if speedups else None
    median_speedup = np.median(speedups) if speedups else None
    # --- Correctness rate: percent of runs where both CARTS and OMP are CORRECT ---
    if not df.empty:
        carts_col = 'carts.correctness_status' if 'carts.correctness_status' in df else None
        omp_col = 'omp.correctness_status' if 'omp.correctness_status' in df else None
        if carts_col and omp_col:
            num_correct = ((df[carts_col] == 'CORRECT') & (df[omp_col] == 'CORRECT')).sum()
            correctness_rate = 100.0 * num_correct / len(df)
        else:
            correctness_rate = None
    else:
        correctness_rate = None
    slowdowns = len([s for s in df.get('speedup_carts_vs_omp', []) if pd.notnull(s) and s < 1])
    unique_examples = len(df['example_name'].unique())
    # Card style tweaks (no icons)
    card_base = {'display': 'inline-block', 'backgroundColor': '#f7fbff', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'padding': '10px 12px', 'margin': '0 8px 8px 0', 'minWidth': '120px', 'textAlign': 'center', 'fontSize': '1.02rem', 'verticalAlign': 'top', 'transition': 'box-shadow 0.2s, transform 0.2s', 'cursor': 'pointer'}
    # Cards
    cards = []
    # Geomean speedup
    cards.append(html.Div([
        html.Div(['Geomean Speedup', html.Span(' ⓘ', title='Geometric mean of all speedups', style={'cursor': 'help', 'color': ACCENT_COLOR, 'fontSize': '1.1em'})], style={'color': PRIMARY_COLOR, 'fontWeight': 'bold'}),
        html.Div(f"{geomean:.2f}x" if geomean else 'N/A', style={'fontSize': '1.2rem', 'fontWeight': 'bold'})
    ], style={**card_base, 'backgroundColor': '#e6f7e6' if geomean and geomean > 1 else '#fffbe6'}))
    # Median speedup
    cards.append(html.Div([
        html.Div(['Median Speedup', html.Span(' ⓘ', title='Median of all speedups', style={'cursor': 'help', 'color': ACCENT_COLOR, 'fontSize': '1.1em'})], style={'color': PRIMARY_COLOR, 'fontWeight': 'bold'}),
        html.Div(f"{median_speedup:.2f}x" if median_speedup else 'N/A', style={'fontSize': '1.2rem', 'fontWeight': 'bold'})
    ], style=card_base))
    # Correctness rate
    cards.append(html.Div([
        html.Div(['Correctness Rate', html.Span(' ⓘ', title='Percentage of runs marked as CORRECT', style={'cursor': 'help', 'color': ACCENT_COLOR, 'fontSize': '1.1em'})], style={'color': PRIMARY_COLOR, 'fontWeight': 'bold'}),
        html.Div(f"{correctness_rate:.1f}%" if correctness_rate is not None else 'N/A', style={'fontSize': '1.2rem', 'fontWeight': 'bold'}),
        html.Div([
            html.Div(style={
                'width': f'{correctness_rate if correctness_rate is not None else 0}%',
                'height': '8px',
                'backgroundColor': '#4caf50',
                'borderRadius': '4px',
                'margin': '4px 0'
            }),
            html.Div(style={
                'width': f'{100 - correctness_rate if correctness_rate is not None else 100}%',
                'height': '8px',
                'backgroundColor': '#f44336',
                'borderRadius': '4px',
                'margin': '4px 0',
                'display': 'inline-block'
            })
        ], style={'width': '100%', 'backgroundColor': '#eee', 'borderRadius': '4px', 'overflow': 'hidden', 'height': '8px', 'margin': '4px 0'})
    ], style={**card_base, 'backgroundColor': '#e6f7e6'}))
    # Unique examples
    cards.append(html.Div([
        html.Div(['Unique Examples', html.Span(' ⓘ', title='Number of unique benchmark examples', style={'cursor': 'help', 'color': ACCENT_COLOR, 'fontSize': '1.1em'})], style={'color': PRIMARY_COLOR, 'fontWeight': 'bold'}),
        html.Div(str(unique_examples), style={'fontSize': '1.2rem', 'fontWeight': 'bold'})
    ], style=card_base))
    # Slowdown card
    if slowdowns:
        cards.append(html.Div([
            html.Div(['Runs with Slowdown', html.Span(' ⓘ', title='Number of runs where speedup < 1', style={'cursor': 'help', 'color': ACCENT_COLOR, 'fontSize': '1.1em'})], style={'color': ACCENT_COLOR, 'fontWeight': 'bold'}),
            html.Div(str(slowdowns), style={'fontSize': '1.2rem', 'fontWeight': 'bold'})
        ], style={**card_base, 'backgroundColor': '#fffbe6'}))
    # Group cards in a flex row, wrap as needed, and center them
    summary_section = html.Div(cards, style={'marginBottom': '10px', 'display': 'flex', 'flexWrap': 'wrap', 'gap': '6px', 'backgroundColor': '#f4f8fc', 'borderRadius': '12px', 'padding': '8px 8px 4px 8px', 'justifyContent': 'center', 'margin': '0 auto'})
    return html.Div([summary_section])

@app.callback(
    [Output('speedup-bar-plot', 'figure'),
     Output('scaling-plot', 'figure'),
     Output('speedup-plot', 'figure'),
     Output('correctness-bar-plot', 'figure'),
     Output('variability-box-plot', 'figure'),
     Output('results-table', 'data'),
     Output('results-table', 'columns')],
    [Input('filters-store', 'data'),
     Input('results-store', 'data'),
     Input('plot-scale-toggle', 'value')]
)
def update_plots_with_filters(filters, data, plot_scale):
    import numpy as np
    import plotly.graph_objs as go
    import plotly.express as px
    if not data:
        empty_fig = go.Figure()
        empty_fig.add_annotation(text="No data to display.", xref="paper", yref="paper", showarrow=False, font=dict(size=18))
        return empty_fig, empty_fig, empty_fig, empty_fig, empty_fig, [], []
    df = pd.DataFrame(data)
    # Filtering logic
    examples = filters.get('examples', [])
    threads = filters.get('threads', [])
    sizes = filters.get('sizes', [])
    correctness = filters.get('correctness', 'ALL')
    dff = df
    if examples:
        dff = dff[dff['example_name'].isin(examples)]
    if threads:
        dff = dff[dff['threads'].isin(threads)]
    if sizes and 'problem_size' in dff.columns:
        dff = dff[dff['problem_size'].isin(sizes)]
    if correctness and correctness != 'ALL':
        if 'correctness' in dff:
            dff = dff[dff['correctness'].str.upper() == correctness]
    required_scaling_cols = ['threads', 'carts.avg_seconds', 'omp.avg_seconds']
    legend_style = dict(
        orientation='h',
        yanchor='bottom',
        y=-0.32,
        xanchor='center',
        x=0.5,
        bgcolor='rgba(255,255,255,0.85)',
        bordercolor='#d0e3f7',
        borderwidth=1,
        font=dict(size=15, color=PRIMARY_COLOR, family='Segoe UI, Arial, sans-serif')
    )
    hovermode = 'x unified'
    plot_height = 600
    # Determine unique problem sizes
    size_col = 'problem_size' if 'problem_size' in dff.columns else None
    unique_sizes = sorted(dff[size_col].unique()) if size_col else []
    n_sizes = len(unique_sizes)
    # Only show size in title if at least one size is selected in the filter
    if sizes and n_sizes > 0:
        size_title = f" (Size: {unique_sizes[0]})" if n_sizes == 1 else (f" (Sizes: {', '.join(str(s) for s in unique_sizes)})" if n_sizes > 1 else "")
    else:
        size_title = ""
    # Speedup: OMP vs CARTS (bar chart per example/thread)
    if 'carts.avg_seconds' in dff.columns and 'omp.avg_seconds' in dff.columns and not dff.empty:
        group_cols = ['example_name', 'threads']
        if size_col:
            group_cols.append('problem_size')
        bar_df = dff.groupby(group_cols, group_keys=False).apply(
            lambda g: pd.Series({
                'speedup_omp_vs_carts': g['omp.avg_seconds'].mean() / g['carts.avg_seconds'].mean()
            })
        ).reset_index()
        color_arg = 'problem_size' if n_sizes > 1 else 'example_name'
        fig_speedup_bar = px.bar(
            bar_df,
            x='threads',
            y='speedup_omp_vs_carts',
            color=color_arg,
            barmode='group',
            labels={'speedup_omp_vs_carts': 'Avg Speedup (OMP / CARTS)', 'threads': 'Threads', 'example_name': 'Example', 'problem_size': 'Size'},
            title=f'Speedup: OMP vs CARTS{size_title}',
            hover_data={"problem_size": True, "example_name": True, "threads": True, "speedup_omp_vs_carts": True}
        )
        fig_speedup_bar.add_hline(y=1, line_dash='dash', line_color='grey')
        fig_speedup_bar.update_layout(height=plot_height, legend=legend_style, title_x=0.5)
    else:
        fig_speedup_bar = go.Figure()
        fig_speedup_bar.add_annotation(text="No speedup data to display.", xref="paper", yref="paper", showarrow=False, font=dict(size=16))
    # Scaling plot
    if all(col in dff.columns for col in required_scaling_cols) and not dff.empty:
        scaling = dff.sort_values('threads')
        # print('Scaling DataFrame shape:', scaling.shape)
        # print('Scaling DataFrame columns:', scaling.columns.tolist())
        # print('threads dtype:', scaling['threads'].dtype)
        # print('threads unique:', scaling['threads'].unique())
        # print('carts.avg_seconds:', scaling['carts.avg_seconds'].tolist())
        # print('omp.avg_seconds:', scaling['omp.avg_seconds'].tolist())
        if not scaling.empty and all(col in scaling.columns for col in ['threads', 'carts.avg_seconds', 'omp.avg_seconds']):
            # Always use long format for scaling
            id_vars = ['threads', 'problem_size', 'example_name'] if 'problem_size' in scaling.columns else ['threads', 'example_name']
            scaling_long = scaling.melt(
                id_vars=id_vars,
                value_vars=['carts.avg_seconds', 'omp.avg_seconds'],
                var_name='version',
                value_name='seconds'
            )
            scaling_long['curve'] = scaling_long['version'].map({'carts.avg_seconds': 'CARTS', 'omp.avg_seconds': 'OMP'})
            if 'problem_size' in scaling_long.columns:
                scaling_long['curve'] = scaling_long['curve'] + ' (size=' + scaling_long['problem_size'].astype(str) + ')'
            fig_scaling = px.line(
                scaling_long,
                x='threads',
                y='seconds',
                color='problem_size',
                line_dash='version',
                labels={'seconds': 'Execution Time (s)', 'threads': 'Threads', 'problem_size': 'Size', 'version': 'Version'},
                title=f'Scaling{size_title}',
                hover_data={"problem_size": True, "example_name": True, "threads": True, "seconds": True, "version": True}
            )
            # Set legend names to "<size>, CARTS" or "<size>, OMP" based on legendgroup and dash
            for trace in fig_scaling.data:
                size_val = trace.legendgroup if hasattr(trace, 'legendgroup') else None
                dash = getattr(trace.line, 'dash', None)
                version = 'CARTS' if dash == 'solid' else 'OMP'
                if size_val is not None:
                    # If legendgroup is like '50,carts.avg_seconds', just take the first part
                    size_only = str(size_val).split(',')[0]
                    trace.name = f"{size_only}, {version}"
                else:
                    trace.name = version
            if plot_scale == 'log':
                fig_scaling.update_yaxes(type='log')
            else:
                fig_scaling.update_yaxes(type='linear')
            fig_scaling.update_layout(height=plot_height, hovermode=hovermode, legend=legend_style, title_x=0.5)
        else:
            fig_scaling = go.Figure()
            fig_scaling.add_annotation(text="No scaling data to display.", xref="paper", yref="paper", showarrow=False, font=dict(size=16))
    else:
        fig_scaling = go.Figure()
        fig_scaling.add_annotation(text="No scaling data to display.", xref="paper", yref="paper", showarrow=False, font=dict(size=16))
    # Speedup plot (strong scaling)
    scaling = dff.sort_values('threads') if not dff.empty else pd.DataFrame()
    # print('Strong Scaling DataFrame shape:', scaling.shape)
    # print('Strong Scaling DataFrame columns:', scaling.columns.tolist())
    # print('threads dtype:', scaling['threads'].dtype)
    # print('threads unique:', scaling['threads'].unique())
    # print('carts.avg_seconds:', scaling['carts.avg_seconds'].tolist())
    # print('omp.avg_seconds:', scaling['omp.avg_seconds'].tolist())
    if not scaling.empty and all(col in scaling.columns for col in ['threads', 'carts.avg_seconds', 'omp.avg_seconds']):
        base_carts = scaling['carts.avg_seconds'].iloc[0]
        base_omp = scaling['omp.avg_seconds'].iloc[0]
        scaling['carts_speedup'] = base_carts / scaling['carts.avg_seconds']
        scaling['omp_speedup'] = base_omp / scaling['omp.avg_seconds']
        # Always use long format for strong scaling
        id_vars = ['threads', 'problem_size', 'example_name'] if 'problem_size' in scaling.columns else ['threads', 'example_name']
        scaling_long = scaling.melt(
            id_vars=id_vars,
            value_vars=['carts_speedup', 'omp_speedup'],
            var_name='version',
            value_name='speedup'
        )
        scaling_long['curve'] = scaling_long['version'].map({'carts_speedup': 'CARTS', 'omp_speedup': 'OMP'})
        if 'problem_size' in scaling_long.columns:
            scaling_long['curve'] = scaling_long['curve'] + ' (size=' + scaling_long['problem_size'].astype(str) + ')'
        fig_speedup = px.line(
            scaling_long,
            x='threads',
            y='speedup',
            color='problem_size',
            line_dash='version',
            labels={'speedup': 'Speedup (vs 1 thread)', 'threads': 'Threads', 'problem_size': 'Size', 'version': 'Version'},
            title=f'Strong Scaling{size_title}',
            hover_data={"problem_size": True, "example_name": True, "threads": True, "speedup": True, "version": True}
        )
        # Set legend names to "<size>, CARTS" or "<size>, OMP" based on legendgroup and dash
        for trace in fig_speedup.data:
            size_val = trace.legendgroup if hasattr(trace, 'legendgroup') else None
            dash = getattr(trace.line, 'dash', None)
            version = 'CARTS' if dash == 'solid' else 'OMP'
            if size_val is not None:
                size_only = str(size_val).split(',')[0]
                trace.name = f"{size_only}, {version}"
            else:
                trace.name = version
        if plot_scale == 'log':
            fig_speedup.update_yaxes(type='log')
        else:
            fig_speedup.update_yaxes(type='linear')
        fig_speedup.update_layout(height=plot_height, hovermode=hovermode, legend=legend_style, title_x=0.5)
    else:
        fig_speedup = go.Figure()
        fig_speedup.add_annotation(text="No speedup data to display.", xref="paper", yref="paper", showarrow=False, font=dict(size=16))
    # Correctness by Example (bar chart)
    carts_col = 'carts.correctness_status' if 'carts.correctness_status' in dff else None
    omp_col = 'omp.correctness_status' if 'omp.correctness_status' in dff else None
    if carts_col and omp_col and not dff.empty:
        corr_df = dff.groupby(['example_name', 'problem_size'] if size_col else ['example_name']).agg({carts_col: lambda x: (x == 'CORRECT').sum(), omp_col: lambda x: (x == 'CORRECT').sum()}).reset_index()
        corr_df = corr_df.rename(columns={carts_col: 'CARTS', omp_col: 'OMP'})
        fig_corr_bar = go.Figure()
        if size_col and n_sizes > 1:
            for sz in unique_sizes:
                sub = corr_df[corr_df['problem_size'] == sz]
                fig_corr_bar.add_trace(go.Bar(x=sub['example_name'], y=sub['CARTS'], name=f'CARTS (Size {sz})', marker_color=PRIMARY_COLOR))
                fig_corr_bar.add_trace(go.Bar(x=sub['example_name'], y=sub['OMP'], name=f'OMP (Size {sz})', marker_color=ACCENT_COLOR))
        else:
            fig_corr_bar.add_trace(go.Bar(x=corr_df['example_name'], y=corr_df['CARTS'], name='CARTS', marker_color=PRIMARY_COLOR))
            fig_corr_bar.add_trace(go.Bar(x=corr_df['example_name'], y=corr_df['OMP'], name='OMP', marker_color=ACCENT_COLOR))
        fig_corr_bar.update_layout(barmode='group', title=f'Correctness by Example{size_title}', xaxis_title='Example', yaxis_title='Correct Runs', height=plot_height, legend=legend_style, title_x=0.5)
    else:
        fig_corr_bar = go.Figure()
        fig_corr_bar.add_annotation(text="No correctness data to display.", xref="paper", yref="paper", showarrow=False, font=dict(size=16))
    # Performance Variability (box plot)
    if 'carts.avg_seconds' in dff.columns and 'omp.avg_seconds' in dff.columns and not dff.empty:
        box_df = dff[['example_name', 'carts.avg_seconds', 'omp.avg_seconds', 'problem_size'] if size_col else ['example_name', 'carts.avg_seconds', 'omp.avg_seconds']].melt(id_vars=['example_name', 'problem_size'] if size_col else 'example_name', var_name='Version', value_name='Execution Time (s)')
        if size_col:
            box_df['Size'] = box_df['problem_size']
        box_df['Version'] = box_df['Version'].map({'carts.avg_seconds': 'CARTS', 'omp.avg_seconds': 'OMP'})
        # Always color by Version for clarity
        fig_var_box = px.box(
            box_df,
            x='example_name',
            y='Execution Time (s)',
            color='Version',
            title=f'Performance Variability (Execution Time Spread){size_title}',
            points='all', height=plot_height,
            hover_data={"Size": True, "example_name": True, "Version": True, "Execution Time (s)": True},
            color_discrete_map={'CARTS': PRIMARY_COLOR, 'OMP': ACCENT_COLOR}
        )
        fig_var_box.update_layout(legend=legend_style, title_x=0.5)
    else:
        fig_var_box = go.Figure()
        fig_var_box.add_annotation(text="No variability data to display.", xref="paper", yref="paper", showarrow=False, font=dict(size=16))
    # Table for selected example/thread/size (or all)
    table_dff = dff.copy()
    for col in table_dff.columns:
        if table_dff[col].apply(lambda x: isinstance(x, list)).any():
            table_dff[col] = table_dff[col].apply(lambda x: ', '.join(str(i) for i in x) if isinstance(x, list) else x)
    columns = [{'name': c, 'id': c} for c in table_dff.columns]
    data = table_dff.to_dict('records')
    return fig_speedup_bar, fig_scaling, fig_speedup, fig_corr_bar, fig_var_box, data, columns

# --- Modal rendering ---
def render_summary_modal(open, card):
    if not open or not card:
        return None
    help_text = CARD_HELP.get(card, "No help available.")
    title_map = {
        'geomean': 'Geomean Speedup',
        'median': 'Median Speedup',
        'correctness': 'Correctness Rate',
        'unique': 'Unique Examples',
        'slowdowns': 'Runs with Slowdown',
    }
    return html.Div([
        html.Div([
            html.Button('×', id='close-modal-btn', n_clicks=0, style=filter_modal_close_style),
            html.H3(title_map.get(card, card), style={'color': PRIMARY_COLOR, 'marginBottom': '12px', 'marginLeft': '32px', 'marginRight': '32px'}),
            html.Div(help_text, style={'color': '#333', 'fontSize': '1.08em', 'marginBottom': '8px', 'marginLeft': '32px', 'marginRight': '32px'}),
        ], style={**modal_content_style}),
    ], style=modal_overlay_style)

@app.callback(
    Output('summary-modal', 'children'),
    [Input('modal-store', 'data')]
)
def update_modal_display(modal_state):
    return render_summary_modal(modal_state.get('open'), modal_state.get('card'))

# --- Filter Modal with pattern-matching close button ---

def render_filter_modal(modal_state, example_opts, thread_opts, correctness_opts, size_opts=None, sysinfo=None):
    # Always robustly set default values to all options if not present
    ex_val = modal_state.get('examples') if modal_state and 'examples' in modal_state else [opt['value'] for opt in example_opts] if example_opts else []
    th_val = modal_state.get('threads') if modal_state and 'threads' in modal_state else [opt['value'] for opt in thread_opts] if thread_opts else []
    sz_val = modal_state.get('sizes') if modal_state and 'sizes' in modal_state else [opt['value'] for opt in size_opts] if size_opts else []
    corr_val = modal_state.get('correctness', 'ALL') if modal_state else 'ALL'
    select_all_style = {'color': ACCENT_COLOR, 'fontWeight': 'bold', 'cursor': 'pointer', 'fontSize': '0.98em', 'marginLeft': '8px', 'textDecoration': 'underline'}
    divider_style = {'height': '1px', 'background': '#e6f0fa', 'margin': '20px 0 14px 0', 'border': 'none'}
    checklist_card_style = {
        'background': '#f7fbff', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)',
        'padding': '10px 10px', 'marginBottom': '16px', 'width': '95%', 'maxHeight': '120px', 'overflowY': 'auto',
        'border': '1.2px solid #d0e3f7', 'fontSize': '1.08rem', 'transition': 'box-shadow 0.2s',
        'fontFamily': 'Segoe UI, Arial, sans-serif', 'wordBreak': 'break-word', 'overflowX': 'hidden',
    }
    radio_card_style = {
        'background': '#f7fbff', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)',
        'padding': '10px 10px', 'marginBottom': '16px', 'width': '95%', 'border': '1.2px solid #d0e3f7', 'fontSize': '1.08rem',
        'fontFamily': 'Segoe UI, Arial, sans-serif', 'wordBreak': 'break-word', 'overflowX': 'hidden',
    }
    accent_bar_style = {
        'height': '7px', 'width': '95%', 'background': f'linear-gradient(90deg, {PRIMARY_COLOR} 0%, {ACCENT_COLOR} 100%)', 'borderTopLeftRadius': '14px', 'borderTopRightRadius': '14px', 'marginBottom': '10px',
    }
    section_header_style = {'fontWeight': 'bold', 'fontSize': '1.08rem', 'color': PRIMARY_COLOR, 'marginTop': '12px', 'marginBottom': '6px', 'letterSpacing': '0.2px'}
    if not example_opts or not thread_opts or not size_opts:
        return html.Div([
            html.Div([
                html.Div(style=accent_bar_style),
                html.Button('×', id={'type': 'close-filter-modal-btn', 'index': 0}, n_clicks=0, style={**filter_modal_close_style, 'fontSize': '2.2em', 'top': '10px', 'right': '18px'}),
                html.H3('Filter Results', style={'color': PRIMARY_COLOR, 'marginBottom': '14px', 'textAlign': 'center', 'fontSize': '1.3rem', 'fontWeight': 'bold'}),
                html.Div('No data loaded. Please load results to use filters.', style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'textAlign': 'center', 'margin': '24px 0', 'fontSize': '1.01rem'})
            ], style={**filter_modal_content_style, 'display': 'flex', 'flexDirection': 'column', 'alignItems': 'center', 'justifyContent': 'center', 'minHeight': '220px', 'maxHeight': '70vh', 'overflowY': 'auto', 'boxShadow': '0 8px 32px rgba(31,119,180,0.18)'}),
        ], style={**filter_modal_overlay_style, 'backdropFilter': 'blur(2px)'})
    return html.Div([
        html.Div([
            html.Div(style=accent_bar_style),
            html.Div([
                html.H3('Filter Results', style={'color': PRIMARY_COLOR, 'marginBottom': '14px', 'textAlign': 'center', 'fontSize': '1.3rem', 'fontWeight': 'bold', 'flex': 1, 'letterSpacing': '0.2px'}),
                html.Button('×', id={'type': 'close-filter-modal-btn', 'index': 0}, n_clicks=0, style={**filter_modal_close_style, 'fontSize': '2.2em', 'top': '10px', 'right': '18px', 'background': 'none', 'border': 'none', 'outline': 'none', 'padding': '0 12px', 'marginLeft': 'auto'}),
            ], style={'display': 'flex', 'alignItems': 'center', 'justifyContent': 'space-between', 'width': '100%', 'marginBottom': '8px'}),
            html.Div([
                html.Label('Examples:', style=section_header_style),
                html.Span('Select All', id='select-all-examples', n_clicks=0, style=select_all_style),
                html.Span(' / ', style={'color': '#aaa', 'margin': '0 2px'}),
                html.Span('Clear All', id='clear-all-examples', n_clicks=0, style=select_all_style),
                dcc.Checklist(id='filter-example-checklist', options=example_opts, value=ex_val, inline=False, style=checklist_card_style),
            ], style={'width': '100%', 'marginBottom': '0', 'marginLeft': '18px', 'marginRight': '18px'}),
            html.Hr(style=divider_style),
            html.Div([
                html.Label('Threads:', style=section_header_style),
                html.Span('Select All', id='select-all-threads', n_clicks=0, style=select_all_style),
                html.Span(' / ', style={'color': '#aaa', 'margin': '0 2px'}),
                html.Span('Clear All', id='clear-all-threads', n_clicks=0, style=select_all_style),
                dcc.Checklist(id='filter-thread-checklist', options=thread_opts, value=th_val, inline=False, style=checklist_card_style),
            ], style={'width': '100%', 'marginBottom': '0', 'marginLeft': '18px', 'marginRight': '18px'}),
            html.Hr(style=divider_style),
            html.Div([
                html.Label('Size:', style=section_header_style),
                html.Span('Select All', id='select-all-sizes', n_clicks=0, style=select_all_style),
                html.Span(' / ', style={'color': '#aaa', 'margin': '0 2px'}),
                html.Span('Clear All', id='clear-all-sizes', n_clicks=0, style=select_all_style),
                dcc.Checklist(id='filter-size-checklist', options=size_opts, value=sz_val, inline=False, style=checklist_card_style),
            ], style={'width': '100%', 'marginBottom': '0', 'marginLeft': '18px', 'marginRight': '18px'}),
            html.Hr(style=divider_style),
            html.Div([
                html.Label('Correctness:', style=section_header_style),
                dcc.RadioItems(id='filter-correctness-radio', options=correctness_opts, value=corr_val, inline=True, style=radio_card_style),
            ], style={'width': '100%', 'marginBottom': '0', 'marginLeft': '18px', 'marginRight': '18px'}),
            html.Div([
                html.Button('Clear All Filters', id={'type': 'reset-filters-btn', 'index': 0}, n_clicks=0, style={'backgroundColor': PRIMARY_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '7px', 'padding': '10px 22px', 'marginRight': '18px', 'fontWeight': 'bold', 'fontSize': '1.01rem', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'transition': 'background 0.2s, box-shadow 0.2s', 'marginTop': '8px'}),
                html.Button('Apply Filters', id={'type': 'apply-filters-btn', 'index': 0}, n_clicks=0, style={'backgroundColor': ACCENT_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '7px', 'padding': '10px 22px', 'fontWeight': 'bold', 'fontSize': '1.01rem', 'boxShadow': '0 2px 8px rgba(255,127,14,0.13)', 'transition': 'background 0.2s, box-shadow 0.2s', 'marginTop': '8px'}),
            ], style={'marginTop': '16px', 'marginBottom': '4px', 'display': 'flex', 'justifyContent': 'center', 'gap': '12px'}),
        ], style={**filter_modal_content_style, 'display': 'flex', 'flexDirection': 'column', 'alignItems': 'center', 'justifyContent': 'center', 'minHeight': '220px', 'maxHeight': '70vh', 'overflowY': 'auto', 'boxShadow': '0 8px 32px rgba(31,119,180,0.18)', 'width': '100%', 'maxWidth': '420px', 'fontSize': '1.00rem', 'padding': '14px 8px 14px 8px', 'fontFamily': 'Segoe UI, Arial, sans-serif', 'marginLeft': '0', 'marginRight': '0', 'background': '#fff', 'borderRadius': '14px', 'border': '1.2px solid #d0e3f7'}),
    ], style={**filter_modal_overlay_style, 'backdropFilter': 'blur(2px)', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'center'})

# --- Add callbacks for Select All / Clear All toggles for each checklist ---
@app.callback(
    Output('filter-example-checklist', 'value'),
    [Input('select-all-examples', 'n_clicks'), Input('clear-all-examples', 'n_clicks')],
    [State('filter-example-checklist', 'options')],
    prevent_initial_call=True
)
def toggle_examples_select_all(select_all, clear_all, options):
    ctx = callback_context
    if not ctx.triggered:
        raise dash.exceptions.PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    if btn_id == 'select-all-examples':
        return [opt['value'] for opt in options] if options else []
    elif btn_id == 'clear-all-examples':
        return []
    raise dash.exceptions.PreventUpdate

@app.callback(
    Output('filter-thread-checklist', 'value'),
    [Input('select-all-threads', 'n_clicks'), Input('clear-all-threads', 'n_clicks')],
    [State('filter-thread-checklist', 'options')],
    prevent_initial_call=True
)
def toggle_threads_select_all(select_all, clear_all, options):
    ctx = callback_context
    if not ctx.triggered:
        raise dash.exceptions.PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    if btn_id == 'select-all-threads':
        return [opt['value'] for opt in options] if options else []
    elif btn_id == 'clear-all-threads':
        return []
    raise dash.exceptions.PreventUpdate

@app.callback(
    Output('filter-size-checklist', 'value'),
    [Input('select-all-sizes', 'n_clicks'), Input('clear-all-sizes', 'n_clicks')],
    [State('filter-size-checklist', 'options')],
    prevent_initial_call=True
)
def toggle_sizes_select_all(select_all, clear_all, options):
    ctx = callback_context
    if not ctx.triggered:
        raise dash.exceptions.PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    if btn_id == 'select-all-sizes':
        return [opt['value'] for opt in options] if options else []
    elif btn_id == 'clear-all-sizes':
        return []
    raise dash.exceptions.PreventUpdate

# --- Split callbacks for filter modal ---
# Callback 1: Open/close/reset modal (no modal controls in State)
@app.callback(
    [Output('filter-modal', 'children'), Output('filter-modal-store', 'data', allow_duplicate=True)],
    [Input('open-filter-modal-btn', 'n_clicks'),
     Input({'type': 'close-filter-modal-btn', 'index': ALL}, 'n_clicks'),
     Input({'type': 'reset-filters-btn', 'index': ALL}, 'n_clicks')],
    [State('filters-store', 'data'),
     State('results-store', 'data'),
     State('filter-modal-store', 'data')],
    prevent_initial_call=True
)
def filter_modal_open_close_reset(open_n, close_n_list, reset_n_list, filters, results, modal_state):
    import pandas as pd
    ctx = callback_context
    if not ctx.triggered:
        raise PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    # Get options for reset
    if results:
        df = pd.DataFrame(results)
        example_opts = [{'label': ex, 'value': ex} for ex in sorted(df['example_name'].unique())] if 'example_name' in df else []
        thread_opts = [{'label': str(t), 'value': t} for t in sorted(df['threads'].unique())] if 'threads' in df else []
        size_opts = [{'label': str(s), 'value': s} for s in sorted(df['problem_size'].unique())] if 'problem_size' in df else []
    else:
        example_opts = []
        thread_opts = []
        size_opts = []
    correctness_opts = [{'label': c, 'value': c} for c in ['ALL', 'CARTS_ONLY', 'OMP_ONLY']]
    # Open modal: use filters-store as initial value, or previous modal state if present
    if btn_id == 'open-filter-modal-btn':
        # If no previous modal state, select all by default
        if not modal_state or not isinstance(modal_state, dict):
            modal_state = {
                'examples': [opt['value'] for opt in example_opts] if example_opts else [],
                'threads': [opt['value'] for opt in thread_opts] if thread_opts else [],
                'sizes': [opt['value'] for opt in size_opts] if size_opts else [],
                'correctness': 'ALL'
            }
        else:
            # If modal_state exists, ensure all keys are present and default to all if missing
            modal_state = {
                'examples': modal_state.get('examples', [opt['value'] for opt in example_opts] if example_opts else []),
                'threads': modal_state.get('threads', [opt['value'] for opt in thread_opts] if thread_opts else []),
                'sizes': modal_state.get('sizes', [opt['value'] for opt in size_opts] if size_opts else []),
                'correctness': modal_state.get('correctness', 'ALL')
            }
        sysinfo = None
        if results and len(results) > 0:
            if isinstance(results, list):
                # Try to get system_information from the first record
                first = results[0]
                if isinstance(first, dict) and 'system_information' in first:
                    sysinfo = first['system_information']
        return render_filter_modal(modal_state, example_opts, thread_opts, correctness_opts, size_opts, sysinfo), modal_state
    # Close modal: just hide it, keep modal state
    elif btn_id.startswith('{') and 'close-filter-modal-btn' in btn_id and any(close_n_list):
        return None, modal_state
    # Reset: keep modal open, reset modal store to all selected
    elif btn_id.startswith('{') and 'reset-filters-btn' in btn_id and any(reset_n_list):
        all_ex = [opt['value'] for opt in example_opts]
        all_th = [opt['value'] for opt in thread_opts]
        all_sz = [opt['value'] for opt in size_opts]
        new_modal_state = {'examples': all_ex, 'threads': all_th, 'sizes': all_sz, 'correctness': 'ALL'}
        sysinfo = None
        if results and len(results) > 0:
            if isinstance(results, list):
                # Try to get system_information from the first record
                first = results[0]
                if isinstance(first, dict) and 'system_information' in first:
                    sysinfo = first['system_information']
        return render_filter_modal(new_modal_state, example_opts, thread_opts, correctness_opts, size_opts, sysinfo), new_modal_state
    # Otherwise, do nothing
    return dash.no_update, modal_state

# Callback 2: Apply Filters (only when modal is open and controls exist)
@app.callback(
    [Output('filters-store', 'data'), Output('filter-modal-store', 'data', allow_duplicate=True), Output('filter-modal', 'children', allow_duplicate=True)],
    [Input({'type': 'apply-filters-btn', 'index': ALL}, 'n_clicks')],
    [State('filter-example-checklist', 'value'),
     State('filter-thread-checklist', 'value'),
     State('filter-correctness-radio', 'value'),
     State('filter-size-checklist', 'value'),
     State('filter-modal-store', 'data')],
    prevent_initial_call=True
)
def apply_filters_callback(apply_n_list, example_val, thread_val, correctness_val, size_val, modal_state):
    ctx = callback_context
    if not ctx.triggered:
        raise PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    if btn_id.startswith('{') and 'apply-filters-btn' in btn_id and any(apply_n_list):
        new_modal_state = {
            'examples': example_val if example_val is not None else [],
            'threads': thread_val if thread_val is not None else [],
            'correctness': correctness_val if correctness_val is not None else 'ALL',
            'sizes': size_val if size_val is not None else [],
        }
        return new_modal_state, new_modal_state, None
    return dash.no_update, dash.no_update, dash.no_update

# Add a function to render the collapsible system info section

def render_system_info(sysinfo):
    if not sysinfo:
        return None
    items = []
    if sysinfo.get('os_platform'):
        items.append(html.Div([html.B('System: '), html.Span(sysinfo.get('os_platform'))]))
    if sysinfo.get('cpu_model'):
        items.append(html.Div([html.B('CPU: '), html.Span(sysinfo.get('cpu_model'))]))
    if sysinfo.get('cpu_cores'):
        items.append(html.Div([html.B('Cores: '), html.Span(str(sysinfo.get('cpu_cores')))]))
    if sysinfo.get('memory_total_mb'):
        items.append(html.Div([html.B('RAM: '), html.Span(str(sysinfo.get('memory_total_mb')) + ' MB')]))
    if sysinfo.get('clang_version'):
        items.append(html.Div([html.B('Clang: '), html.Span(sysinfo.get('clang_version'))]))
    if sysinfo.get('timestamp'):
        items.append(html.Div([html.B('Timestamp: '), html.Span(sysinfo.get('timestamp'))]))
    return html.Details([
        html.Summary('System Information', style={'fontWeight': 'bold', 'color': ACCENT_COLOR, 'fontSize': '1.1rem', 'marginTop': '8px'}),
        html.Div(items, style={'marginTop': '10px', 'fontSize': '0.98rem', 'color': '#333'})
    ], open=False, style={'marginBottom': '18px', 'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '12px 18px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)'})

# Add a callback to render the system info section below the metadata panel
@app.callback(
    Output('system-info-card', 'children'),
    [Input('results-path', 'value')]
)
def update_system_info_card(results_path):
    if not results_path or not os.path.exists(results_path):
        return None
    with open(results_path) as f:
        data = json.load(f)
    sysinfo = data.get('system_information', None)
    return render_system_info(sysinfo)

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=8050)