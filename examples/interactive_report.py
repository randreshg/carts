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
import plotly.graph_objs as go
import io
import base64
import re
import traceback

# --- File Paths ---
DEFAULT_RESULTS_PATH = os.path.join(os.path.dirname(__file__), 'output/performance_results.json')

# --- Utility Functions ---
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

def flatten_lists_in_df(df):
    for col in df.columns:
        if df[col].apply(lambda x: isinstance(x, list)).any():
            df[col] = df[col].apply(lambda x: ', '.join(map(str, x)) if isinstance(x, list) else x)
    return df

# --- Styling ---
# (Keep only the most relevant style dicts, remove duplicates)
MAIN_BG_COLOR = '#f5f9fc'
CARD_BG_COLOR = '#ffffff'
PRIMARY_COLOR = '#2c6fbb'
ACCENT_COLOR = '#ff7f0e'
SUCCESS_COLOR = '#36b37e'
WARNING_COLOR = '#ffab00'
NEUTRAL_COLOR = '#505f79'

# --- PlotTemplate class for consistent, parametric plot styling ---
class PlotTemplate:
    def __init__(self,
                 legend_orientation='h', legend_y=-0.35, legend_x=0.5,
                 height=600, margin=None, bargap=0.18,
                 grid_color='#cccccc', axis_color='#888888',
                 bar_line_width=2, bar_width=0.4, line_width=2, marker_line_width=1,
                 font_family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif', font_size=15,
                 xaxis_title='', yaxis_title='', plot_title='', legend_title=''):
        if margin is None:
            margin = dict(l=40, r=40, t=40, b=120)
        self.legend_orientation = legend_orientation
        self.legend_y = legend_y
        self.legend_x = legend_x
        self.height = height
        self.margin = margin
        self.bargap = bargap
        self.grid_color = grid_color
        self.axis_color = axis_color
        self.bar_line_width = bar_line_width
        self.bar_width = bar_width
        self.line_width = line_width
        self.marker_line_width = marker_line_width
        self.font_family = font_family
        self.font_size = font_size
        self.xaxis_title = xaxis_title
        self.yaxis_title = yaxis_title
        self.plot_title = plot_title
        self.legend_title = legend_title

    def apply(self, fig, xaxis_title=None, yaxis_title=None, plot_title=None, legend_title=None):
        fig.update_layout(
            height=self.height,
            title=plot_title if plot_title is not None else self.plot_title,
            title_x=0.5,
            font=dict(family=self.font_family, size=self.font_size),
            legend=dict(
                title=legend_title if legend_title is not None else self.legend_title,
                orientation=self.legend_orientation,
                yanchor='bottom' if self.legend_orientation == 'h' else 'top',
                y=self.legend_y,
                xanchor='center' if self.legend_orientation == 'h' else 'right',
                x=self.legend_x,
                bgcolor='white',
                bordercolor='#bbbbbb',
                borderwidth=1,
                font=dict(size=14)
            ),
            plot_bgcolor='white',
            paper_bgcolor='white',
            margin=self.margin,
            bargap=self.bargap,
            xaxis=dict(
                title=dict(text=xaxis_title if xaxis_title is not None else self.xaxis_title, font=dict(size=14, color=NEUTRAL_COLOR)),
                tickfont=dict(size=12),
                gridcolor=self.grid_color,
                showgrid=True,
                showline=True,
                linecolor=self.axis_color,
                ticks='outside',
            ),
            yaxis=dict(
                title=dict(text=yaxis_title if yaxis_title is not None else self.yaxis_title, font=dict(size=14, color=NEUTRAL_COLOR)),
                tickfont=dict(size=12),
                gridcolor=self.grid_color,
                showgrid=True,
                showline=True,
                linecolor=self.axis_color,
                ticks='outside',
            ),
        )
        for trace in fig.data:
            if trace.type == 'bar':
                color = PRIMARY_COLOR
                if hasattr(trace, 'marker') and hasattr(trace.marker, 'line') and hasattr(trace.marker.line, 'color') and trace.marker.line.color:
                    color = trace.marker.line.color
                trace.marker.color = 'white'
                trace.marker.line.color = color
                trace.marker.line.width = self.bar_line_width
                trace.width = self.bar_width
            if trace.type == 'scatter':
                trace.line.width = self.line_width
                trace.marker.line.width = self.marker_line_width
                trace.marker.line.color = self.axis_color
        return fig

    def profiling_x_label(self, row):
        # Always use 'T=threads | S=size' if size exists, else just threads
        # Accept both Series and dict, and both lowercase and capitalized keys
        if hasattr(row, 'to_dict'):
            row = row.to_dict()
        threads = row.get('threads', row.get('Threads', None))
        problem_size = row.get('problem_size', row.get('Problem Size', None))
        if problem_size is not None and pd.notnull(problem_size):
            return f"T={threads} | S={problem_size}"
        else:
            return f"T={threads}"

# --- Instantiate a default template ---
default_plot_template = PlotTemplate(
    yaxis_title='Value',
    legend_title='Example - Version'
)

main_div_style = {
    'background': 'linear-gradient(135deg, #f5f9fc 0%, #ffffff 100%)',
    'minHeight': '100vh',
    'fontFamily': 'Segoe UI, Arial, sans-serif',
    'color': NEUTRAL_COLOR,
}
card_style = {
    'backgroundColor': CARD_BG_COLOR,
    'borderRadius': '16px',
    'boxShadow': '0 8px 24px rgba(0,0,0,0.08)',
    'padding': '32px 32px 24px 32px',
    'margin': '24px auto',
    'maxWidth': '1400px',
    'border': '1px solid #f0f0f0',
}
header_style = {
    'color': PRIMARY_COLOR,
    'fontWeight': '600',
    'fontSize': '2.6rem',
    'marginBottom': '12px',
    'textAlign': 'center',
    'letterSpacing': '0.5px',
    'padding': '24px 0 18px 0',
    'borderBottom': f'2px solid {PRIMARY_COLOR}',
    'background': 'transparent',
    'position': 'relative',
}
subtitle_style = {
    'color': NEUTRAL_COLOR,
    'fontSize': '1.3rem',
    'textAlign': 'center',
    'marginBottom': '28px',
    'fontWeight': '400',
    'maxWidth': '900px',
    'margin': '0 auto 28px auto',
}
label_style = {
    'fontWeight': '600',
    'color': PRIMARY_COLOR,
    'marginTop': '14px',
    'fontSize': '1.1rem',
    'letterSpacing': '0.2px',
}
filter_modal_close_style = {
    'position': 'absolute',
    'top': '10px',
    'right': '18px',
    'background': 'none',
    'border': 'none',
    'outline': 'none',
    'padding': '0 12px',
    'marginLeft': 'auto',
}
filter_modal_content_style = {
    'backgroundColor': '#fff',
    'borderRadius': '14px',
    'boxShadow': '0 8px 32px rgba(31,119,180,0.18)',
    'padding': '14px 8px 14px 8px',
    'fontFamily': 'Segoe UI, Arial, sans-serif',
    'fontSize': '1.00rem',
    'width': '100%',
    'maxWidth': '420px',
    'marginLeft': '0',
    'marginRight': '0',
    'border': '1.2px solid #d0e3f7',
}
filter_modal_overlay_style = {
    'position': 'fixed',
    'top': 0,
    'left': 0,
    'width': '100vw',
    'height': '100vh',
    'background': 'rgba(0,0,0,0.18)',
    'zIndex': 1000,
}

# --- Layout ---
app = dash.Dash(__name__, suppress_callback_exceptions=True)
app.layout = html.Div([
    dcc.Store(id='modal-store', data={'open': False, 'card': None}),
    dcc.Store(id='filters-store', data={'examples': [], 'threads': []}),
    dcc.Store(id='filter-modal-store'),
    dcc.Store(id='results-type-store', data='both'),
    html.Div([
        html.Div([
            html.H1([
                html.Span("CARTS", style={'color': PRIMARY_COLOR, 'fontWeight': '700'}),
                " Performance Dashboard"
            ], style=header_style),
            html.Div("Benchmark Comparison between CARTS and OpenMP Implementations", style=subtitle_style),
        ], style={'marginBottom': '12px', 'paddingBottom': '4px'}),
        dcc.Store(id='results-store'),
        html.Div([
            html.Label([
                html.I(className="fas fa-file-alt", style={'marginRight': '8px'}),
                "Results file:",
                html.Span(" ⓘ", title="Path to the results JSON file (e.g., output/performance_results.json)", style={'cursor': 'help', 'color': ACCENT_COLOR, 'marginLeft': '4px'})
            ], style={**label_style, 'width': 'auto', 'textAlign': 'right', 'marginRight': '15px', 'marginBottom': '0', 'alignSelf': 'center', 'minWidth': '120px'}),
            dcc.Input(
                id='results-path',
                type='text',
                value=DEFAULT_RESULTS_PATH,
                style={
                    'width': '60%',
                    'marginRight': '12px',
                    'marginBottom': '0',
                    'textAlign': 'left',
                    'height': '42px',
                    'lineHeight': '42px',
                    'padding': '0 15px',
                    'borderRadius': '8px',
                    'border': '1px solid #e0e0e0',
                    'boxShadow': '0 2px 8px rgba(0,0,0,0.05)',
                    'fontSize': '1.05rem',
                    'alignSelf': 'center'
                }
            ),
        ], style={'marginBottom': '16px', 'marginTop': '12px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'center', 'gap': '0'}),
        html.Div(id='data-status-message', style={'color': ACCENT_COLOR, 'fontWeight': '600', 'marginBottom': '16px', 'textAlign': 'center'}),
        html.Div([
            html.Button([
                html.I(className="fas fa-filter", style={'marginRight': '8px'}),
                "Filter Results"
            ], id='open-filter-modal-btn', n_clicks=0, style={'backgroundColor': PRIMARY_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '8px', 'padding': '12px 36px', 'fontWeight': '600', 'margin': '0 auto', 'display': 'block', 'boxShadow': '0 4px 12px rgba(44,111,187,0.15)', 'cursor': 'pointer', 'fontSize': '1.05rem'}),
        ], style={'display': 'flex', 'justifyContent': 'center', 'alignItems': 'center', 'marginBottom': '28px', 'marginTop': '12px'}),
        html.Div(id='average-per-example-bar-section'),
        # --- Main Performance Plots Section (wrapped in a styled card) ---
        html.Div([
            html.H2("Benchmark Results", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '8px', 'marginBottom': '12px'}),
            html.Div(id='main-tabs-section'),
        ], style={
            'backgroundColor': '#fff',
            'borderRadius': '14px',
            'boxShadow': '0 4px 16px rgba(44,111,187,0.08)',
            'padding': '18px 18px 10px 18px',
            'marginBottom': '24px',
            'border': '1.5px solid #e0e0e0'
        }),
        html.Div(id='filter-modal'),
        html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),
        html.Div(id='profiling-summary-section'),
        html.H2("Profiling Analysis", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '18px', 'marginBottom': '10px'}),
        html.Div([
            html.Label('Event Group:', style={'fontWeight': 'bold', 'color': PRIMARY_COLOR, 'fontSize': '1.08em', 'marginRight': '10px'}),
            dcc.Dropdown(id='profiling-group-dropdown', options=[], value=None, clearable=False, style={'width': '260px', 'display': 'inline-block', 'marginBottom': '0', 'fontSize': '1.08em'})
        ], style={'display': 'flex', 'alignItems': 'center', 'marginBottom': '12px', 'marginTop': '2px'}),
        html.Div(id='perf-event-analysis-section'),
        html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),
        html.H2("Results Table", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '24px', 'marginBottom': '12px'}),
        html.Div(id='results-table-section'),
        html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),
        html.Div(id='derived-metrics-section'),  # <-- Add this line for the new section
        html.Div(id='system-info-card'),
        html.Div(id='summary-modal'),
        dcc.Dropdown(id='perf-event-dropdown', style={'display': 'none'}),
    ], style=card_style),
    html.Div([
        html.Span("CARTS Benchmarking Report | "),
        html.A("GitHub", href="https://github.com/ARTS-Lab/carts", target="_blank", style={'color': PRIMARY_COLOR, 'textDecoration': 'none', 'fontWeight': 'bold'}),
        html.Span(" | For scientific reproducibility. "),
        html.Span("© 2024")
    ], style={'textAlign': 'center', 'color': NEUTRAL_COLOR, 'marginTop': '40px', 'fontSize': '1.05rem', 'paddingBottom': '20px', 'borderTop': '1px solid #e0e0e0', 'paddingTop': '20px', 'maxWidth': '1400px', 'margin': '40px auto 0 auto'}),
], style=main_div_style)

# --- Callbacks ---
# (Keep only important comments, clarify logic)
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
    [Output('main-tabs-section', 'children'), Output('results-table-section', 'children')],
    [Input('filters-store', 'data'), Input('results-store', 'data')]
)
def render_graph_tabs_and_table(filters, data):
    empty_fig = go.Figure()
    if not data:
        figs = [empty_fig] * 5
        table = html.Div("No data to display.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
    else:
        df = pd.DataFrame(data)
        df = flatten_lists_in_df(df)
        dff = df
        if filters.get('examples'):
            dff = dff[dff['example_name'].isin(filters['examples'])]
        if filters.get('threads'):
            dff = dff[dff['threads'].isin(filters['threads'])]
        if filters.get('sizes') and 'problem_size' in dff.columns:
            dff = dff[dff['problem_size'].isin(filters['sizes'])]
        if filters.get('correctness') and filters['correctness'] != 'ALL':
            if 'correctness' in dff:
                dff = dff[dff['correctness'].str.upper() == filters['correctness']]
        carts_col = next((c for c in dff.columns if c.lower().startswith('carts') and 'avg_seconds' in c), None)
        omp_col = next((c for c in dff.columns if c.lower().startswith('omp') and 'avg_seconds' in c), None)
        # Scaling plot
        scaling_fig = empty_fig
        if not dff.empty and 'threads' in dff.columns and carts_col and omp_col:
            # Remove code name from x_label, only show threads and size
            dff['x_label'] = dff.apply(lambda row: f"T={row['threads']} | S={row['problem_size']}" if 'problem_size' in dff.columns else f"T={row['threads']}", axis=1)
            def _try_parse_size(val):
                try:
                    return float(val)
                except Exception:
                    return str(val)
            dff = dff.copy()
            dff['threads_sort'] = pd.to_numeric(dff['threads'], errors='coerce')
            dff['size_sort'] = dff['problem_size'].apply(_try_parse_size) if 'problem_size' in dff.columns else 0
            dff = dff.sort_values(['threads_sort', 'size_sort'])
            marker_symbols = ['circle', 'square', 'diamond', 'cross', 'x', 'triangle-up', 'triangle-down', 'star']
            scaling_data = []
            legend_labels = []
            for i, (version, color) in enumerate([(carts_col, PRIMARY_COLOR), (omp_col, ACCENT_COLOR)]):
                if version and version in dff.columns:
                    for j, ex in enumerate(dff['example_name'].unique()):
                        ex_df = dff[dff['example_name'] == ex]
                        legend = f"{ex} - {version.split('.')[0].upper()}"
                        legend_labels.append(legend)
                        symbol = marker_symbols[(i * len(dff['example_name'].unique()) + j) % len(marker_symbols)]
                        if 'problem_size' in dff.columns and ex_df['problem_size'].nunique() > 1:
                            for sz in sorted(ex_df['problem_size'].unique(), key=_try_parse_size):
                                sz_df = ex_df[ex_df['problem_size'] == sz]
                                if not sz_df.empty and version in sz_df.columns:
                                    sz_df = sz_df.copy()
                                    sz_df['customdata'] = sz_df.apply(lambda row: [row['threads'], row['problem_size']], axis=1)
                                    sz_df = sz_df.sort_values(['threads_sort', 'size_sort'])
                                    scaling_data.append(go.Scatter(
                                        x=sz_df['x_label'], y=sz_df[version], mode='lines+markers',
                                        name=legend,
                                        line=dict(color=color, width=2),
                                        marker=dict(symbol=symbol, size=10, line=dict(width=1, color='black')),
                                        customdata=sz_df['customdata'],
                                        hovertemplate='<b>Threads: %{customdata[0]}</b><br>Size: %{customdata[1]}<br>Time: %{y:,.4f} s<extra>%{fullData.name}</extra>'
                                    ))
                        else:
                            if not ex_df.empty and version in ex_df.columns:
                                ex_df = ex_df.copy()
                                ex_df['customdata'] = ex_df.apply(lambda row: [row['threads'], row['problem_size']], axis=1)
                                ex_df = ex_df.sort_values(['threads_sort', 'size_sort'])
                                scaling_data.append(go.Scatter(
                                    x=ex_df['x_label'], y=ex_df[version], mode='lines+markers',
                                    name=legend,
                                    line=dict(color=color, width=2),
                                    marker=dict(symbol=symbol, size=10, line=dict(width=1, color='black')),
                                    customdata=ex_df['customdata'],
                                    hovertemplate='<b>Threads: %{customdata[0]}</b><br>Size: %{customdata[1]}<br>Time: %{y:,.4f} s<extra>%{fullData.name}</extra>'
                                ))
            scaling_fig = go.Figure(scaling_data)
            # --- Use the template for all plots ---
            scaling_fig = default_plot_template.apply(
                scaling_fig,
                xaxis_title='Threads | Size',
                yaxis_title='Execution Time (s)',
                plot_title='Scaling: Execution Time'
            )
        # Speedup plot
        speedup_bar_fig = empty_fig
        if not dff.empty and carts_col and omp_col:
            dff['x_label'] = dff.apply(lambda row: f"T={row['threads']} | S={row['problem_size']}" if 'problem_size' in dff.columns else f"T={row['threads']}", axis=1)
            dff = dff.copy()
            dff['threads_sort'] = pd.to_numeric(dff['threads'], errors='coerce')
            dff['size_sort'] = dff['problem_size'].apply(_try_parse_size) if 'problem_size' in dff.columns else 0
            dff = dff.sort_values(['threads_sort', 'size_sort'])
            avg_df = dff.groupby(['x_label', 'example_name']).agg({carts_col: 'mean', omp_col: 'mean', 'threads': 'first', 'problem_size': 'first', 'threads_sort': 'first', 'size_sort': 'first'}).reset_index()
            avg_df['Speedup'] = avg_df[carts_col] / avg_df[omp_col]
            avg_df['legend_label'] = avg_df['example_name']
            speedup_data = []
            for j, ex in enumerate(avg_df['example_name'].unique()):
                ex_df = avg_df[avg_df['example_name'] == ex]
                legend = f"{ex} - Speedup"
                speedup_data.append(go.Scatter(
                    x=ex_df['x_label'], y=ex_df['Speedup'],
                    mode='lines+markers',
                    name=legend,
                    line=dict(color=PRIMARY_COLOR if j % 2 == 0 else ACCENT_COLOR, width=2),
                    marker=dict(size=10, line=dict(width=1, color='black')),
                    customdata=ex_df[['threads', 'problem_size']].values,
                    hovertemplate='<b>Threads: %{customdata[0]}</b><br>Size: %{customdata[1]}<br>Speedup: %{y:,.4f}<extra>%{fullData.name}</extra>'
                ))
            speedup_bar_fig = go.Figure(speedup_data)
            speedup_bar_fig = default_plot_template.apply(
                speedup_bar_fig,
                xaxis_title='Threads | Size',
                yaxis_title='Speedup (CARTS / OMP)',
                plot_title='Speedup: CARTS / OMP',
                legend_title='Example'
            )
        # Variability plot
        var_data = []
        if not dff.empty and carts_col and omp_col:
            timing_rows = []
            import json as _json
            # Extract all timing samples for each version
            for version, label in [('carts', 'CARTS'), ('omp', 'OMP')]:
                times_col = f'{version}.times_seconds'
                if times_col in dff.columns:
                    for _, row in dff.iterrows():
                        times = row[times_col]
                        if isinstance(times, str):
                            try:
                                # Try JSON first
                                times = _json.loads(times.replace("'", '"'))
                            except Exception:
                                # Fallback: parse comma-separated string
                                times = [float(x.strip()) for x in times.split(',') if x.strip()]
                        if times and isinstance(times, list):
                            for t in times:
                                timing_rows.append({
                                    'example_name': row['example_name'],
                                    'threads': row['threads'],
                                    'problem_size': row.get('problem_size', None),
                                    'version': label,
                                    'timing_value': t
                                })
            timing_df = pd.DataFrame(timing_rows)
            if not timing_df.empty:
                try:
                    # Sort for consistent x-axis
                    def _try_parse_size(val):
                        try:
                            return float(val)
                        except Exception:
                            return str(val)
                    timing_df['threads_sort'] = pd.to_numeric(timing_df['threads'], errors='coerce')
                    timing_df['size_sort'] = timing_df['problem_size'].apply(_try_parse_size) if 'problem_size' in timing_df.columns else 0
                    timing_df = timing_df.sort_values(['threads_sort', 'size_sort'])
                    # Use template logic for x_label and legend_label
                    def safe_profiling_x_label(row):
                        # row may be a Series; convert to dict
                        d = row.to_dict() if hasattr(row, 'to_dict') else dict(row)
                        # Fallbacks for missing keys
                        threads = d.get('threads', d.get('Threads', None))
                        problem_size = d.get('problem_size', d.get('Problem Size', None))
                        return default_plot_template.profiling_x_label({'threads': threads, 'problem_size': problem_size})
                    timing_df['x_label'] = timing_df.apply(safe_profiling_x_label, axis=1)
                    timing_df['legend_label'] = timing_df['example_name'].astype(str) + ' - ' + timing_df['version'].astype(str)
                    # Compute mean and stddev for each group
                    group_cols = ['x_label', 'legend_label', 'threads', 'problem_size']
                    stats_df = timing_df.groupby(group_cols)['timing_value'].agg(['mean', 'std']).reset_index()
                    # Box plot (distribution)
                    box_fig = go.Figure()
                    for legend in timing_df['legend_label'].unique():
                        legend_df = timing_df[timing_df['legend_label'] == legend]
                        box_fig.add_trace(go.Box(
                            x=legend_df['x_label'],
                            y=legend_df['timing_value'],
                            name=legend,
                            boxmean=False,
                            marker=dict(line=dict(width=2, color='black')),
                            line=dict(width=2),
                            jitter=0.35,
                            pointpos=0,
                            boxpoints='all',
                            hovertemplate='<b>%{x}</b><br>Version: %{name}<br>Time: %{y:,.4f} s<extra></extra>'
                        ))
                    # Overlay mean as a marker (diamond)
                    try:
                        for _, row in stats_df.iterrows():
                            box_fig.add_trace(go.Scatter(
                                x=[row['x_label']],
                                y=[row['mean']],
                                mode='markers',
                                marker=dict(symbol='diamond', size=14, color='black', line=dict(width=2, color='white')),
                                name=f"Mean - {row['legend_label']}",
                                showlegend=False,
                                hovertemplate=f"<b>{row['x_label']}</b><br>Mean: {row['mean']:.4f} s<br>Stddev: {row['std']:.4f} s<extra></extra>"
                            ))
                    except Exception as exc:
                        traceback.print_exc()
                    # Use the default plot template for consistent style
                    box_fig = default_plot_template.apply(
                        box_fig,
                        xaxis_title='Threads | Size',
                        yaxis_title='Execution Time (s)',
                        plot_title='Variability of Execution Time',
                        legend_title='Example - Version'
                    )
                    variability_fig = box_fig
                except Exception as e:
                    traceback.print_exc()
                    variability_fig = go.Figure()
                    variability_fig.add_annotation(text=f"Plot error: {str(e)}", xref="paper", yref="paper", showarrow=False, font=dict(color="red"))
                    variability_fig.update_layout(title='Variability of Execution Time ', title_x=0.5)
            else:
                variability_fig = go.Figure()
                variability_fig.add_annotation(text="No timing data available.", xref="paper", yref="paper", showarrow=False, font=dict(size=16))
                variability_fig.update_layout(title='Variability of Execution Time ', title_x=0.5)
        else:
            variability_fig = go.Figure()
            variability_fig.add_annotation(text="No timing data available.", xref="paper", yref="paper", showarrow=False, font=dict(size=16))
            variability_fig.update_layout(title='Variability of Execution Time ', title_x=0.5)
        figs = [scaling_fig, speedup_bar_fig, variability_fig]
        tabs = dcc.Tabs([
            dcc.Tab(label='Scaling', children=[dcc.Graph(figure=figs[0], config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})]),
            dcc.Tab(label='Speedup', children=[dcc.Graph(figure=figs[1], config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})]),
            dcc.Tab(label='Variability', children=[dcc.Graph(figure=figs[2], config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})]),
        ], style={'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '12px 18px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'marginBottom': '18px'})
        # --- Add profiling averages to the results table ---
        # For each row, compute the mean of each profiling event for CARTS and OMP, and add as new columns
        def extract_event_avgs(row, version):
            stats = row.get(version, {}).get('event_stats', {})
            avgs = {}
            for event, ev_stats in stats.items():
                event_clean = event.replace(":u", "")
                avg = ev_stats.get('avg')
                if avg is not None:
                    avgs[f'avg_{event_clean}_{version}'] = avg
            return avgs
        if not dff.empty:
            dff = flatten_lists_in_df(dff)
            # Build a new DataFrame with averages and summary columns
            rows = []
            all_event_names = set()
            for _, row in dff.iterrows():
                base = row.to_dict()
                avgs_carts = extract_event_avgs(row, 'carts')
                avgs_omp = extract_event_avgs(row, 'omp')
                all_event_names.update([k.replace('avg_', '').replace('_carts', '').replace('_omp', '') for k in avgs_carts.keys()])
                all_event_names.update([k.replace('avg_', '').replace('_carts', '').replace('_omp', '') for k in avgs_omp.keys()])
                # Add summary columns for both versions
                base['avg_seconds_carts'] = row.get('carts', {}).get('avg_seconds') if isinstance(row.get('carts'), dict) else None
                base['stdev_seconds_carts'] = row.get('carts', {}).get('stdev_seconds') if isinstance(row.get('carts'), dict) else None
                base['correctness_carts'] = row.get('carts', {}).get('correctness_status') if isinstance(row.get('carts'), dict) else None
                base['avg_seconds_omp'] = row.get('omp', {}).get('avg_seconds') if isinstance(row.get('omp'), dict) else None
                base['stdev_seconds_omp'] = row.get('omp', {}).get('stdev_seconds') if isinstance(row.get('omp'), dict) else None
                base['correctness_omp'] = row.get('omp', {}).get('correctness_status') if isinstance(row.get('omp'), dict) else None
                base['speedup_carts_vs_omp'] = row.get('speedup_carts_vs_omp')
                base.update(avgs_carts)
                base.update(avgs_omp)
                rows.append(base)
            compact_df = pd.DataFrame(rows)
            # Always include all possible avg_* columns for all events seen
            avg_cols = []
            for event in sorted(all_event_names):
                avg_cols.append(f'avg_{event}_carts')
                avg_cols.append(f'avg_{event}_omp')
            # Only keep key columns, summary columns, and avg_* columns
            key_cols = [c for c in ['example_name', 'problem_size', 'threads', 'args', 'correctness'] if c in compact_df.columns]
            summary_cols = [c for c in ['avg_seconds_carts', 'stdev_seconds_carts', 'correctness_carts', 'avg_seconds_omp', 'stdev_seconds_omp', 'correctness_omp', 'speedup_carts_vs_omp'] if c in compact_df.columns]
            show_cols = key_cols + summary_cols + avg_cols
            # Remove duplicates while preserving order
            seen = set()
            unique_show_cols = []
            for col in show_cols:
                if col not in seen:
                    unique_show_cols.append(col)
                    seen.add(col)
            # Fill missing columns with N/A
            for col in unique_show_cols:
                if col not in compact_df.columns:
                    compact_df[col] = 'N/A'
            columns = [{"name": i, "id": i} for i in unique_show_cols]
            table = dash_table.DataTable(
                data=compact_df[unique_show_cols].to_dict('records'),
                columns=columns,
                page_size=20,
                style_table={'overflowX': 'auto'},
                style_cell={'textAlign': 'left', 'fontSize': '1.01rem'},
                style_header={'backgroundColor': '#e6f0fa', 'fontWeight': 'bold'},
            )
        else:
            table = html.Div("No data to display.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
    # --- Derived Metrics Table ---
    derived_table = None
    try:
        # Build a profiling events DataFrame (like profiling_summary_table)
        profiling_rows = []
        for row in data:
            example = row.get("example_name")
            problem_size = row.get("problem_size")
            threads = row.get("threads")
            args = row.get("args", "")
            for version in ["carts", "omp"]:
                stats = row.get(version, {}).get("event_stats", {})
                for event, ev_stats in stats.items():
                    event_clean = event.replace(":u", "")
                    profiling_rows.append({
                        "Example": example,
                        "Problem Size": problem_size,
                        "Threads": threads,
                        "Version": version.upper(),
                        "Event": event_clean,
                        "Avg": ev_stats.get("avg")
                    })
        profiling_df = pd.DataFrame(profiling_rows)
        derived_df = derived_metrics_table(profiling_df if not profiling_df.empty else pd.DataFrame())
        if not derived_df.empty:
            derived_columns = [
                {"name": i, "id": i} for i in derived_df.columns
            ]
            derived_table = html.Div([
                html.H3("Derived Metrics (e.g., MPKI, CPI, Miss Rates, IPC)", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '18px', 'marginBottom': '10px'}),
                dash_table.DataTable(
                    data=derived_df.to_dict('records'),
                    columns=derived_columns,
                    page_size=20,
                    style_table={'overflowX': 'auto'},
                    style_cell={'textAlign': 'left', 'fontSize': '1.01rem'},
                    style_header={'backgroundColor': '#e6f0fa', 'fontWeight': 'bold'},
                )
            ], style={'backgroundColor': '#f9fafc', 'borderRadius': '8px', 'padding': '12px 18px', 'marginBottom': '18px'})
    except Exception as e:
        import traceback
        print('DEBUG: Exception in derived_metrics_table:', e)
        traceback.print_exc()
        derived_table = None
    # Compose the results section
    results_section = html.Div([
        table,
        derived_table if derived_table is not None else None
    ])
    return tabs, results_section

# --- Filter Modal with pattern-matching close button ---
def render_filter_modal(modal_state, example_opts, thread_opts, size_opts=None, sysinfo=None):
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
        return render_filter_modal(modal_state, example_opts, thread_opts, size_opts, sysinfo), modal_state
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
        return render_filter_modal(new_modal_state, example_opts, thread_opts, size_opts, sysinfo), new_modal_state
    # Otherwise, do nothing
    return dash.no_update, modal_state

# Callback 2: Apply Filters (only when modal is open and controls exist)
@app.callback(
    [Output('filters-store', 'data'), Output('filter-modal-store', 'data', allow_duplicate=True), Output('filter-modal', 'children', allow_duplicate=True)],
    [Input({'type': 'apply-filters-btn', 'index': ALL}, 'n_clicks')],
    [State('filter-example-checklist', 'value'),
     State('filter-thread-checklist', 'value'),
     State('filter-size-checklist', 'value'),
     State('filter-modal-store', 'data')],
    prevent_initial_call=True
)
def apply_filters_callback(apply_n_list, example_val, thread_val, size_val, modal_state):
    ctx = callback_context
    if not ctx.triggered:
        raise PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    if btn_id.startswith('{') and 'apply-filters-btn' in btn_id and any(apply_n_list):
        new_modal_state = {
            'examples': example_val if example_val is not None else [],
            'threads': thread_val if thread_val is not None else [],
            'sizes': size_val if size_val is not None else [],
        }
        return new_modal_state, new_modal_state, None
    return dash.no_update, dash.no_update, dash.no_update

# Add a function to render the system info section below the metadata panel
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
    return html.Div([
        html.H2("System Information", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '24px', 'marginBottom': '12px'}),
        html.Div(items, style={'marginTop': '10px', 'fontSize': '0.98rem', 'color': '#333', 'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '12px 18px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)'})
    ], style={'marginBottom': '18px'})

# --- Optimized average per-example bar plot ---
def average_per_example_figure(df):
    if df is None or df.empty:
        fig = go.Figure()
        fig.add_annotation(text="No data to display.", xref="paper", yref="paper", showarrow=False, 
                          font=dict(size=18, color=NEUTRAL_COLOR, family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif'))
        fig.update_layout(
            paper_bgcolor='rgba(0,0,0,0)',
            plot_bgcolor='rgba(0,0,0,0)',
            height=500
        )
        return fig
        
    # Use vectorized groupby/agg
    carts_col = next((c for c in df.columns if c.lower().startswith('carts') and 'avg_seconds' in c), None)
    omp_col = next((c for c in df.columns if c.lower().startswith('omp') and 'avg_seconds' in c), None)
    
    if not carts_col or not omp_col:
        fig = go.Figure()
        fig.add_annotation(text="No timing data to display.", xref="paper", yref="paper", showarrow=False, 
                          font=dict(size=16, color=NEUTRAL_COLOR, family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif'))
        fig = default_plot_template.apply(fig)
        return fig
        
    avg_df = df.groupby('example_name', as_index=False)[[carts_col, omp_col]].mean()
    
    # Create readable labels for the legend
    carts_label = "CARTS"
    omp_label = "OpenMP"
    
    fig = go.Figure()
    fig.add_trace(go.Scatter(
        x=avg_df['example_name'], y=avg_df[carts_col],
        mode='lines+markers',
        name=carts_label,
        line=dict(color=PRIMARY_COLOR, width=2),
        marker=dict(size=10, line=dict(width=1, color='black')),
        hovertemplate='<b>%{x}</b><br>Execution Time: %{y:,.4f} seconds<extra>%{fullData.name}</extra>'
    ))
    fig.add_trace(go.Scatter(
        x=avg_df['example_name'], y=avg_df[omp_col],
        mode='lines+markers',
        name=omp_label,
        line=dict(color=ACCENT_COLOR, width=2),
        marker=dict(size=10, line=dict(width=1, color='black')),
        hovertemplate='<b>%{x}</b><br>Execution Time: %{y:,.4f} seconds<extra>%{fullData.name}</extra>'
    ))
    fig = default_plot_template.apply(
        fig,
        xaxis_title='Example',
        yaxis_title='Execution Time (seconds)',
        plot_title='Average Execution Time Comparison',
        legend_title='Version'
    )
    return fig

# --- Optimized callback for average-per-example bar ---
@app.callback(
    Output('average-per-example-bar', 'children'),
    [Input('results-store', 'data')]
)
def update_average_per_example_bar(data):
    if not data:
        return html.Div()
    df = pd.DataFrame(data)
    fig = average_per_example_figure(df)
    return dcc.Graph(figure=fig, config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})

def profiling_summary_table(profiling_results):
    # Flatten profiling results for table
    rows = []
    for r in profiling_results:
        example = r.get("example_name")
        problem_size = r.get("problem_size")
        threads = r.get("threads")
        args = r.get("args", "")
        for version in ["carts", "omp"]:
            stats = r.get(version, {}).get("event_stats", {})
            for event, ev_stats in stats.items():
                row = {
                    "Example": example,
                    "Problem Size": problem_size,
                    "Threads": threads,
                    "Args": args,
                    "Version": version.upper(),
                    "Event": event,
                    "Min": ev_stats.get("min"),
                    "Max": ev_stats.get("max"),
                    "Avg": ev_stats.get("avg"),
                    "Median": ev_stats.get("median"),
                    "Stdev": ev_stats.get("stdev")
                }
                rows.append(row)
    df = pd.DataFrame(rows)
    return df

def profiling_event_bar_chart(df, event, version):
    dff = df[(df['Event'] == event) & (df['Version'] == version.upper())]
    # Robust check: if dff is empty or missing required columns, return empty fig
    if dff.empty or not all(col in dff.columns for col in ['Example', 'Avg', 'Threads']):
        fig = go.Figure()
        fig.add_annotation(text="No data for this event/version.", xref="paper", yref="paper", showarrow=False)
        return fig
    # Ensure Threads has at least one non-null value and is string type
    if dff['Threads'].isnull().all():
        fig = go.Figure()
        fig.add_annotation(text="No valid thread data for this event/version.", xref="paper", yref="paper", showarrow=False)
        return fig
    dff = dff.copy()
    dff['x_label'] = dff.apply(lambda row: default_plot_template.profiling_x_label(row), axis=1)
    dff['legend_label'] = dff['Example'].astype(str) + ' - ' + dff['Version'].astype(str)
    try:
        fig = go.Figure()
        for legend in dff['legend_label'].unique():
            dff_legend = dff[dff['legend_label'] == legend]
            fig.add_trace(go.Scatter(
                x=dff_legend['x_label'], y=dff_legend['Avg'],
                mode='lines+markers',
                name=legend,
                line=dict(width=2),
                marker=dict(size=10, line=dict(width=1, color='black')),
                customdata=dff_legend[['Example', 'Threads', 'Problem Size']].values,
                hovertemplate='<b>%{customdata[0]}</b><br>Threads: %{customdata[1]}<br>Size: %{customdata[2]}<br>Avg: %{y:,.4f}<extra></extra>'
            ))
        fig = default_plot_template.apply(
            fig,
            xaxis_title='Threads | Size' if 'Problem Size' in dff.columns else 'Threads',
            yaxis_title=event,
            plot_title=f'{event} (avg) - {version.upper()} (Line Chart)'
        )
    except Exception as e:
        fig = go.Figure()
        fig.add_annotation(text=f"Plot error: {str(e)}", xref="paper", yref="paper", showarrow=False, font=dict(color="red"))
    return fig

def profiling_event_heatmap(df, event, version):
    # Heatmap: Example vs Threads, colored by avg value
    dff = df[(df['Event'] == event) & (df['Version'] == version.upper())]
    if dff.empty:
        fig = go.Figure()
        fig.add_annotation(text="No data for this event/version.", xref="paper", yref="paper", showarrow=False)
        return fig
    pivot = dff.pivot_table(index='Example', columns='Threads', values='Avg', aggfunc='mean')
    fig = px.imshow(
        pivot,
        labels=dict(x="Threads", y="Example", color=event),
        title=f"{event} (avg) Heatmap - {version.upper()}"
    )
    fig.update_traces(
        hovertemplate='<b>Example: %{y}</b><br>Threads: %{x}<br>Avg: %{z:,.4f}<extra></extra>'
    )
    fig.update_layout(
        height=420,
        title_x=0.5,
        font=dict(family='Segoe UI, Arial, sans-serif', size=15),
        plot_bgcolor='#f9fafc',
        paper_bgcolor='#fff',
        margin=dict(l=40, r=20, t=40, b=40),
    )
    return fig

def derived_metrics_table(df):
    # Compute MPKI, CPI, bandwidth, cache miss rates, IPC, stalls per instruction
    derived = []
    for _, row in df.iterrows():
        event = row['Event']
        # MPKI (Misses per Kilo Instructions)
        if event.endswith('-misses'):
            instr = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'instructions')]
            if not instr.empty and instr.iloc[0]['Avg']:
                mpki = (row['Avg'] / instr.iloc[0]['Avg']) * 1000
                derived.append({**row, 'Metric': f"{event} MPKI", 'Value': mpki})
        # CPI (Cycles per Instruction)
        if event == 'cycles':
            instr = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'instructions')]
            if not instr.empty and row['Avg'] and instr.iloc[0]['Avg']:
                cpi = row['Avg'] / instr.iloc[0]['Avg']
                derived.append({**row, 'Metric': 'CPI', 'Value': cpi})
        # Bandwidth estimation (if loads/stores and cycles are available)
        if event in ['loads', 'stores']:
            cycles = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'cycles')]
            if not cycles.empty and row['Avg'] and cycles.iloc[0]['Avg']:
                bw = row['Avg'] / cycles.iloc[0]['Avg']  # proxy: accesses per cycle
                derived.append({**row, 'Metric': f'{event} per cycle', 'Value': bw})
        # IPC (Instructions per Cycle)
        if event == 'instructions':
            cycles = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'cycles')]
            if not cycles.empty and row['Avg'] and cycles.iloc[0]['Avg']:
                ipc = row['Avg'] / cycles.iloc[0]['Avg']
                derived.append({**row, 'Metric': 'IPC', 'Value': ipc})
        # Cache miss rates
        if event == 'L1-dcache-loads':
            misses = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'L1-dcache-load-misses')]
            if not misses.empty and row['Avg'] and misses.iloc[0]['Avg']:
                miss_rate = misses.iloc[0]['Avg'] / row['Avg']
                derived.append({**row, 'Metric': 'L1 Miss Rate', 'Value': miss_rate})
        if event == 'L2-dcache-loads':
            misses = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'L2-dcache-load-misses')]
            if not misses.empty and row['Avg'] and misses.iloc[0]['Avg']:
                miss_rate = misses.iloc[0]['Avg'] / row['Avg']
                derived.append({**row, 'Metric': 'L2 Miss Rate', 'Value': miss_rate})
        if event == 'L3-dcache-loads':
            misses = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'L3-dcache-load-misses')]
            if not misses.empty and row['Avg'] and misses.iloc[0]['Avg']:
                miss_rate = misses.iloc[0]['Avg'] / row['Avg']
                derived.append({**row, 'Metric': 'L3 Miss Rate', 'Value': miss_rate})
        # Stalls per instruction
        if event == 'stalled-cycles-frontend':
            instr = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'instructions')]
            if not instr.empty and row['Avg'] and instr.iloc[0]['Avg']:
                stalls_per_instr = row['Avg'] / instr.iloc[0]['Avg']
                derived.append({**row, 'Metric': 'Frontend Stalls/Instr', 'Value': stalls_per_instr})
        if event == 'stalled-cycles-backend':
            instr = df[(df['Example'] == row['Example']) & (df['Threads'] == row['Threads']) & (df['Version'] == row['Version']) & (df['Event'] == 'instructions')]
            if not instr.empty and row['Avg'] and instr.iloc[0]['Avg']:
                stalls_per_instr = row['Avg'] / instr.iloc[0]['Avg']
                derived.append({**row, 'Metric': 'Backend Stalls/Instr', 'Value': stalls_per_instr})
    return pd.DataFrame(derived)

# Add callbacks to populate and filter by problem size and args in profiling tab

@app.callback(
    [Output('profiling-size-dropdown', 'options'),
     Output('profiling-size-dropdown', 'value')],
    [Input('results-store', 'data')]
)
def update_profiling_size_dropdown(data):
    raise PreventUpdate

# NOTE: The profiling dropdown callbacks are under development and disabled for now to keep the app runnable.
# @app.callback(
#     [Output('profiling-size-dropdown', 'options'),
#      Output('profiling-size-dropdown', 'value')],
#     [Input('results-store', 'data')]
# )
# def update_profiling_size_dropdown(data):
#     raise PreventUpdate

# --- Perf Event Grouping ---
EVENT_GROUPS = {
    "Cache": [
        "L1-dcache-loads", "L1-dcache-load-misses", "L1-dcache-stores", "L1-icache-load-misses",
        "L2-dcache-loads", "L2-dcache-load-misses", "L3-dcache-loads", "L3-dcache-load-misses",
        "cache-references", "cache-misses",
        # Add l2_rqsts and mem_inst_retired events
        "l2_rqsts.all_code_rd", "l2_rqsts.code_rd_hit", "l2_rqsts.code_rd_miss", "l2_rqsts.miss",
        "mem_inst_retired.all_loads", "mem_inst_retired.all_stores"
    ],
    "Clocks/Frequency": ["cycles", "cpu-clock", "task-clock"],
    "Stalls": ["stalled-cycles-frontend", "stalled-cycles-backend", "cycle_activity.stalls_total", "cycle_activity.stalls_mem_any", "resource_stalls.sb", "resource_stalls.scoreboard"],
    "TLB": ["dTLB-load-misses", "iTLB-load-misses", "dTLB-store-misses"],
    "Instructions": ["instructions"],
    "Other": []  # Will be filled dynamically
}

# --- Perf Event Descriptions ---
PERF_EVENT_DESCRIPTIONS = {
    "L1-dcache-loads": "L1 data cache load accesses.",
    "L1-dcache-load-misses": "L1 data cache load misses.",
    "L1-dcache-stores": "L1 data cache store accesses.",
    "L1-icache-load-misses": "L1 instruction cache load misses.",
    "L2-dcache-loads": "L2 data cache load accesses.",
    "L2-dcache-load-misses": "L2 data cache load misses.",
    "L3-dcache-loads": "L3 data cache load accesses.",
    "L3-dcache-load-misses": "L3 data cache load misses.",
    "cache-references": "Cache accesses of any level (read/write).",
    "cache-misses": "Cache misses at any cache level.",
    "l2_rqsts.all_code_rd": "L2 code read requests.",
    "l2_rqsts.code_rd_hit": "L2 code read hits.",
    "l2_rqsts.code_rd_miss": "L2 code read misses.",
    "l2_rqsts.miss": "L2 cache misses (all requests).",
    "mem_inst_retired.all_loads": "Retired memory load instructions.",
    "mem_inst_retired.all_stores": "Retired memory store instructions.",
    "cycles": "Total CPU cycles.",
    "cpu-clock": "CPU clock count (software event).",
    "task-clock": "Task clock count (software event).",
    "stalled-cycles-frontend": "Cycles stalled in the frontend (instruction fetch/decode).",
    "stalled-cycles-backend": "Cycles stalled in the backend (execution/memory).",
    "cycle_activity.stalls_total": "Total cycles with any stall.",
    "cycle_activity.stalls_mem_any": "Cycles stalled due to memory subsystem.",
    "resource_stalls.sb": "Cycles stalled due to store buffer full.",
    "resource_stalls.scoreboard": "Cycles stalled due to scoreboard (resource conflicts).",
    "dTLB-load-misses": "Data TLB load misses.",
    "iTLB-load-misses": "Instruction TLB load misses.",
    "dTLB-store-misses": "Data TLB store misses.",
    "instructions": "Retired instructions. Each instruction executed by the CPU.",
}

def group_event(event):
    for group, events in EVENT_GROUPS.items():
        if event in events:
            return group
    return "Other"

@app.callback(
    Output('perf-event-analysis-section', 'children'),
    [Input('results-store', 'data'),
     Input('profiling-group-dropdown', 'value'),
     Input('filters-store', 'data')],
)
def update_perf_event_analysis_section(data, selected_group=None, filters=None):
    if not data:
        return html.Div("No profiling data available.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold'})
    import pandas as pd
    df_flat = pd.DataFrame(data)
    # Apply filters if present (same logic as main plots)
    if filters:
        if filters.get('examples'):
            df_flat = df_flat[df_flat['example_name'].isin(filters['examples'])]
        if filters.get('threads'):
            df_flat = df_flat[df_flat['threads'].isin(filters['threads'])]
        if filters.get('sizes') and 'problem_size' in df_flat.columns:
            df_flat = df_flat[df_flat['problem_size'].isin(filters['sizes'])]
        if filters.get('correctness') and filters['correctness'] != 'ALL':
            if 'correctness' in df_flat:
                df_flat = df_flat[df_flat['correctness'].str.upper() == filters['correctness']]
    profiling_rows = []
    for idx, row in df_flat.iterrows():
        example = row.get("example_name")
        problem_size = row.get("problem_size")
        threads = row.get("threads")
        args = row.get("args", "")
        for version in ["carts", "omp"]:
            prefix = f"{version}.event_stats."
            for col in df_flat.columns:
                if col.startswith(prefix) and col.endswith(".avg"):
                    event = col[len(prefix):-4]
                    avg = row[col]
                    if pd.notnull(avg):
                        profiling_rows.append({
                            "Example": example,
                            "Problem Size": problem_size,
                            "Threads": threads,
                            "Args": args,
                            "Version": version.upper(),
                            "Event": event.replace(":u", ""),
                            "Avg": avg
                        })
    df = pd.DataFrame(profiling_rows)
    if df.empty or 'Event' not in df:
        return html.Div("No profiling data available.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold'})
    # Group events
    event_groups = {g: [] for g in EVENT_GROUPS}
    for event in df['Event'].unique():
        group = group_event(event)
        event_groups.setdefault(group, []).append(event)
    for event in df['Event'].unique():
        if not any(event in v for v in EVENT_GROUPS.values()):
            event_groups['Other'].append(event)
    # Dropdown for group selection
    group_options = [{'label': g, 'value': g} for g in event_groups if event_groups[g]]
    if not selected_group or selected_group not in event_groups or not event_groups[selected_group]:
        selected_group = group_options[0]['value'] if group_options else None
    # Shorten event names for tabs
    def short_event_name(event):
        # Remove common prefixes and abbreviate
        return event.replace('L1-dcache-', 'L1d-').replace('L2-dcache-', 'L2d-').replace('L3-dcache-', 'L3d-').replace('icache-', 'ic-').replace('load-misses', 'ld-miss').replace('loads', 'ld').replace('stores', 'st').replace('misses', 'miss').replace('references', 'ref').replace('instructions', 'instr').replace('cycles', 'cyc').replace('cpu-clock', 'cpuclk').replace('task-clock', 'tskclk').replace('stalled-cycles-frontend', 'stl-cyc-frontend').replace('stalled-cycles-backend', 'stl-cyc-backend').replace('resource_stalls.sb', 'rs.sb').replace('resource_stalls.scoreboard', 'rs.score').replace('cycle_activity.stalls_total', 'cyc.stl.tot').replace('cycle_activity.stalls_mem_any', 'cyc.stl.mem').replace('mem_inst_retired.all_loads', 'mem.ld').replace('mem_inst_retired.all_stores', 'mem.st').replace('cache-references', 'cache-ref').replace('cache-misses', 'cache-miss').replace('dTLB-load-misses', 'dTLB-ld-miss').replace('iTLB-load-misses', 'iTLB-ld-miss').replace('dTLB-store-misses', 'dTLB-st-miss')
    # Sub-tabs for each event in the selected group
    event_tabs = []
    for event in event_groups[selected_group]:
        dff = df[df['Event'] == event]
        # Use real perf description if available, else fallback
        event_desc = PERF_EVENT_DESCRIPTIONS.get(event, f"Shows the average value of {event} for each example, thread, and size configuration.")
        info_line = html.Div([
            html.Span("ⓘ", style={'color': '#888', 'marginRight': '6px'}),
            html.Span(f"{event}: {event_desc}", style={'color': '#888', 'fontSize': '0.98em'})
        ], style={'marginBottom': '8px'})
        if dff.empty:
            event_tabs.append(dcc.Tab(
                label=short_event_name(event),
                children=[
                    info_line,
                    html.Div("No data for this event.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold'})
                ],
                style={'fontSize': '1.01em', 'padding': '6px 10px'}
            ))
            continue
        try:
            dff = dff.copy()
            dff['x_label'] = dff.apply(lambda row: default_plot_template.profiling_x_label(row), axis=1)
            dff['legend_label'] = dff['Example'].astype(str) + ' - ' + dff['Version'].astype(str)
            dff['customdata'] = dff.apply(lambda row: [row['Example'], row['Threads'], row.get('Problem Size', None)], axis=1)
            fig = go.Figure()
            for legend in dff['legend_label'].unique():
                dff_legend = dff[dff['legend_label'] == legend]
                fig.add_trace(go.Scatter(
                    x=dff_legend['x_label'], y=dff_legend['Avg'],
                    mode='lines+markers',
                    name=legend,
                    line=dict(width=2),
                    marker=dict(size=10, line=dict(width=1, color='black')),
                    customdata=dff_legend[['Example', 'Threads', 'Problem Size']].values,
                    hovertemplate='<b>%{customdata[0]}</b><br>Threads: %{customdata[1]}<br>Size: %{customdata[2]}<br>Avg: %{y:,.4f}<extra></extra>'
                ))
            fig = default_plot_template.apply(
                fig,
                xaxis_title='Threads | Size' if 'Problem Size' in dff.columns else 'Threads',
                yaxis_title=event,
                plot_title=f'{short_event_name(event)}'
            )
            event_tabs.append(dcc.Tab(
                label=short_event_name(event),
                children=[
                    info_line,
                    dcc.Graph(figure=fig, config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})
                ],
                style={'fontSize': '1.01em', 'padding': '6px 10px'}
            ))
        except Exception as e:
            event_tabs.append(dcc.Tab(
                label=short_event_name(event),
                children=[
                    info_line,
                    html.Div(f"Plot error: {str(e)}", style={'color': 'red'})
                ],
                style={'fontSize': '1.01em', 'padding': '6px 10px'}
            ))
    return html.Div([
        html.Div([
            dcc.Tabs(event_tabs, style={'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '8px 8px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'marginBottom': '18px', 'fontSize': '1.01em'})
        ]),
    ], style={'backgroundColor': '#fff', 'borderRadius': '14px', 'boxShadow': '0 4px 16px rgba(44,111,187,0.08)', 'padding': '18px 18px 10px 18px', 'marginBottom': '24px', 'border': '1.5px solid #e0e0e0'})

# Add a new section for profiling summary above the results table
@app.callback(
    Output('profiling-summary-section', 'children'),
    [Input('results-store', 'data')]
)
def update_profiling_summary_section(data):
    if not data:
        return None
    profiling_rows = []
    for row in data:
        example = row.get("example_name")
        problem_size = row.get("problem_size")
        threads = row.get("threads")
        args = row.get("args", "")
        for version in ["carts", "omp"]:
            stats = row.get(version, {}).get("event_stats", {})
            for event, ev_stats in stats.items():
                event_clean = event.replace(":u", "")
                profiling_rows.append({
                    "Example": example,
                    "Problem Size": problem_size,
                    "Threads": threads,
                    "Version": version.upper(),
                    "Event": event_clean,
                    "Avg": ev_stats.get("avg")
                })
    import pandas as pd
    df = pd.DataFrame(profiling_rows)
    if not df.empty and 'Avg' in df.columns:
        df = df[df['Avg'].notnull()]
    if df.empty:
        return None
    # Show a summary table of all events/values
    columns = [
        {"name": i, "id": i} for i in ["Example", "Problem Size", "Threads", "Version", "Event", "Avg"]
    ]
    table = dash_table.DataTable(
        data=df.to_dict('records'),
        columns=columns,
        page_size=10,
        style_table={'overflowX': 'auto'},
        style_cell={'textAlign': 'left', 'fontSize': '1.01rem'},
        style_header={'backgroundColor': '#e6f0fa', 'fontWeight': 'bold'},
    )
    return html.Div([
        html.H3("Profiling Event Summary", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '18px', 'marginBottom': '10px'}),
        table
    ], style={'backgroundColor': '#f9fafc', 'borderRadius': '8px', 'padding': '12px 18px', 'marginBottom': '18px'})

# Add a callback to update the dropdown options and value based on available groups
@app.callback(
    [Output('profiling-group-dropdown', 'options'), Output('profiling-group-dropdown', 'value')],
    [Input('results-store', 'data')]
)
def update_profiling_group_dropdown(data):
    import pandas as pd
    if not data:
        return [], None
    df_flat = pd.DataFrame(data)
    profiling_rows = []
    for idx, row in df_flat.iterrows():
        for version in ["carts", "omp"]:
            prefix = f"{version}.event_stats."
            for col in df_flat.columns:
                if col.startswith(prefix) and col.endswith(".avg"):
                    event = col[len(prefix):-4]
                    if pd.notnull(row[col]):
                        profiling_rows.append({"Event": event.replace(":u", "")})
    df = pd.DataFrame(profiling_rows)
    if df.empty or 'Event' not in df:
        return [], None
    event_groups = {g: [] for g in EVENT_GROUPS}
    for event in df['Event'].unique():
        group = group_event(event)
        event_groups.setdefault(group, []).append(event)
    for event in df['Event'].unique():
        if not any(event in v for v in EVENT_GROUPS.values()):
            event_groups['Other'].append(event)
    group_options = [{'label': g, 'value': g} for g in event_groups if event_groups[g]]
    value = group_options[0]['value'] if group_options else None
    return group_options, value

# --- Add a common plotly style helper ---
def plotly_common_layout(fig, legend_orientation='h', legend_y=-0.35, legend_x=0.5, height=600, margin=None, bargap=0.18):
    if margin is None:
        margin = dict(l=40, r=40, t=40, b=120)
    fig.update_layout(
        height=height,
        title_x=0.5,
        font=dict(family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif', size=15),
        legend=dict(
            orientation=legend_orientation,
            yanchor='bottom' if legend_orientation == 'h' else 'top',
            y=legend_y,
            xanchor='center' if legend_orientation == 'h' else 'right',
            x=legend_x,
            bgcolor='white',
            bordercolor='black',
            borderwidth=1,
            font=dict(size=14)
        ),
        plot_bgcolor='white',
        paper_bgcolor='white',
        margin=margin,
        bargap=bargap,
        xaxis=dict(
            title=dict(font=dict(size=14, color=NEUTRAL_COLOR)),
            tickfont=dict(size=12),
            gridcolor='#000',
            showgrid=True,
            showline=True,
            linecolor='black',
            ticks='outside',
        ),
        yaxis=dict(
            title=dict(font=dict(size=14, color=NEUTRAL_COLOR)),
            tickfont=dict(size=12),
            gridcolor='#000',
            showgrid=True,
            showline=True,
            linecolor='black',
            ticks='outside',
        ),
    )
    # Bar/line style: white fill, colored border for bars, thick lines
    for trace in fig.data:
        if trace.type == 'bar':
            # Use color from marker.line.color if set, else fallback
            color = PRIMARY_COLOR
            if hasattr(trace, 'marker') and hasattr(trace.marker, 'line') and hasattr(trace.marker.line, 'color') and trace.marker.line.color:
                color = trace.marker.line.color
            trace.marker.color = 'white'
            trace.marker.line.color = color
            trace.marker.line.width = 3
            trace.width = 0.4
        if trace.type == 'scatter':
            trace.line.width = 2
            trace.marker.line.width = 1
            trace.marker.line.color = 'black'
    return fig

# --- Derived Metrics Section Callback ---
@app.callback(
    Output('derived-metrics-section', 'children'),
    [Input('results-store', 'data')]
)
def render_derived_metrics_section(data):
    print("DEBUG: render_derived_metrics_section called. Data present?", bool(data))
    if not data:
        return None
    profiling_rows = []
    for row in data:
        example = row.get("example_name")
        problem_size = row.get("problem_size")
        threads = row.get("threads")
        args = row.get("args", "")
        for version in ["carts", "omp"]:
            stats = row.get(version, {}).get("event_stats", {})
            for event, ev_stats in stats.items():
                event_clean = event.replace(":u", "")
                profiling_rows.append({
                    "Example": example,
                    "Problem Size": problem_size,
                    "Threads": threads,
                    "Version": version.upper(),
                    "Event": event_clean,
                    "Avg": ev_stats.get("avg")
                })
    import pandas as pd
    profiling_df = pd.DataFrame(profiling_rows)
    derived_df = derived_metrics_table(profiling_df if not profiling_df.empty else pd.DataFrame())
    if not derived_df.empty:
        print("DEBUG: Derived metrics table will be rendered. Rows:", len(derived_df))
        derived_columns = [
            {"name": i, "id": i} for i in derived_df.columns
        ]
        return html.Div([
            html.H3("Derived Metrics (e.g., MPKI, CPI, Miss Rates, IPC)", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '18px', 'marginBottom': '10px'}),
            dash_table.DataTable(
                data=derived_df.to_dict('records'),
                columns=derived_columns,
                page_size=20,
                style_table={'overflowX': 'auto'},
                style_cell={'textAlign': 'left', 'fontSize': '1.01rem'},
                style_header={'backgroundColor': '#e6f0fa', 'fontWeight': 'bold'},
            )
        ], style={'backgroundColor': '#f9fafc', 'borderRadius': '8px', 'padding': '12px 18px', 'marginBottom': '18px'})
    print("DEBUG: No derived metrics available (no profiling data found).")
    return html.Div("No derived metrics available (no profiling data found).", style={'color': ACCENT_COLOR, 'fontWeight': 'bold'})

if __name__ == "__main__":
    app.run(debug=True)