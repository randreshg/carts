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
import ast
from dash.dependencies import ALL

# --- File Paths ---
DEFAULT_RESULTS_PATH = os.path.join(os.path.dirname(__file__), 'output/performance_results.json')

# --- Utility Functions ---
def load_results(path):
    if not os.path.exists(path):
        return None, f"Results file not found: {path}"
    try:
        with open(path) as f:
            data = json.load(f)
        # Merge both benchmark and profiling results
        results = []
        if 'benchmark_results' in data and isinstance(data['benchmark_results'], list):
            results.extend(data['benchmark_results'])
        if 'profiling_results' in data and isinstance(data['profiling_results'], list):
            results.extend(data['profiling_results'])
        if not results:
            return None, "No results found in JSON."
        df = pd.json_normalize(results)
        return df, None
    except Exception as e:
        return None, f"Error loading file: {e}"

def flatten_lists_in_df(df):
    for col in df.columns:
        if df[col].apply(lambda x: isinstance(x, list)).any():
            df[col] = df[col].apply(lambda x: ', '.join(map(str, x)) if isinstance(x, list) else x)
    return df

# --- Utility function to apply filters to a DataFrame --- #
def apply_dashboard_filters(df, filters):
    dff = df
    if filters.get('examples'):
        dff = dff[dff['example_name'].isin(filters['examples'])]
    if filters.get('threads'):
        dff = dff[dff['threads'].isin(filters['threads'])]
    if filters.get('sizes') and 'problem_size' in dff.columns:
        dff = dff[dff['problem_size'].isin(filters['sizes'])]
    if filters.get('correctness') and filters['correctness'] != 'ALL':
        if 'correctness_status' in dff:
            dff = dff[dff['correctness_status'].str.upper() == filters['correctness']]
    return dff

# --- Styling --- # 
MAIN_BG_COLOR = '#f5f9fc'
CARD_BG_COLOR = '#ffffff'
PRIMARY_COLOR = '#2c6fbb'
ACCENT_COLOR = '#ff7f0e'
SUCCESS_COLOR = '#36b37e'
WARNING_COLOR = '#ffab00'
NEUTRAL_COLOR = '#505f79'

# --- PlotTemplate class --- #
class PlotTemplate:
    def __init__(self,
                 legend_orientation='h', legend_y=-0.35, legend_x=0.5,  # Legend at bottom
                 height=600, margin=None, bargap=0.18,
                 grid_color='#cccccc', axis_color='#888888',
                 bar_line_width=2, bar_width=0.4, line_width=2, marker_line_width=1,
                 font_family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif', font_size=15,
                 xaxis_title='', yaxis_title='', plot_title='', legend_title=''):
        if margin is None:
            margin = dict(l=40, r=40, t=50, b=180)  # Larger bottom margin for legend
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
        margin = self.margin.copy() if isinstance(self.margin, dict) else dict(l=40, r=40, t=120, b=180)
        # Always set title as dict with x=0.5 for centering
        title_text = plot_title if plot_title is not None else self.plot_title
        fig.update_layout(
            height=self.height,
            title=dict(text=title_text, x=0.5),
            font=dict(family=self.font_family, size=self.font_size),
            legend=dict(
                title=legend_title if legend_title is not None else self.legend_title,
                orientation=self.legend_orientation,
                yanchor='top',  # Legend at bottom
                y=self.legend_y,
                xanchor='center',
                x=self.legend_x,
                bgcolor='white',
                bordercolor='#bbbbbb',
                borderwidth=1,
                font=dict(size=14)
            ),
            plot_bgcolor='white',
            paper_bgcolor='white',
            margin=margin,
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

# --- Instantiate a default template --- #
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

# --- Layout --- #
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
        # ---  Benchmark Results Section --- #
        html.Div([
            html.H2("Benchmark Results", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '8px', 'marginBottom': '12px'}),
            html.Div(id='main-tabs-section'),
            html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),
            html.H2("Summary Table", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '24px', 'marginBottom': '12px'}),
            html.Button("Download Benchmark Summary CSV", id="download-benchmark-btn", n_clicks=0, style={
                'backgroundColor': ACCENT_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '7px',
                'padding': '8px 18px', 'fontWeight': 'bold', 'fontSize': '1.01rem', 'marginBottom': '10px'}),
            dcc.Download(id="download-benchmark-csv"),
            html.Div(id='results-table-section'),
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

        # ---  Profiling Analysis Section --- #
        html.Div([
            html.H2("Profiling Analysis", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '8px', 'marginBottom': '12px'}),
            dcc.Dropdown(id='profiling-group-dropdown', options=[], value='Cache', clearable=False, style={'width': '260px', 'display': 'inline-block', 'marginBottom': '0', 'fontSize': '1.08em'}),
            html.Div(id='perf-event-analysis-section'),
            html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '25px 0'}),
            html.Div(id='profiling-summary-section'),
        ], style={
            'backgroundColor': '#fff',
            'borderRadius': '14px',
            'boxShadow': '0 4px 16px rgba(44,111,187,0.08)',
            'padding': '18px 18px 10px 18px',
            'marginBottom': '24px',
            'border': '1.5px solid #e0e0e0'
        }),
        html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),

        

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

# --- Callbacks --- #
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
        figs = [empty_fig] * 4
        table = html.Div("No data to display.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
    else:
        df = pd.DataFrame(data)
        df = flatten_lists_in_df(df)
        dff = apply_dashboard_filters(df, filters)
        baseline_df = dff[dff['run_type'] == 'no_profile']
        profile_df = dff[dff['run_type'] == 'profile']
        # --- Scaling plot --- #
        scaling_fig = empty_fig
        if not baseline_df.empty:
            scaling_fig = go.Figure()
            for (example, version), group in baseline_df.groupby(['example_name', 'version']):
                group = group.copy()
                group['x_label'] = group.apply(lambda row: default_plot_template.profiling_x_label(row), axis=1)
                stats = compute_timing_stats(group, group_cols=['example_name', 'problem_size', 'threads', 'version', 'run_type'])
                legend_name = f"{example} - {version}"
                scaling_fig.add_trace(go.Scatter(
                    x=group['x_label'],
                    y=stats['mean'],
                    mode='lines+markers',
                    name=legend_name,
                    hovertemplate='<b>%{x}</b><br>Mean Time: %{y:.4f} s<br>Example: '+example+'<br>Version: '+version+'<extra></extra>'
                ))
            scaling_fig = default_plot_template.apply(
                scaling_fig,
                xaxis_title='Threads | Size',
                yaxis_title='Mean Time (s)',
                plot_title='Scaling'
            )
        # --- Speedup plot --- #
        speedup_bar_fig = empty_fig
        if not baseline_df.empty:
            speedup_rows = []
            for (example, problem_size, threads), group in baseline_df.groupby(['example_name', 'problem_size', 'threads']):
                carts = group[group['version'] == 'CARTS']
                omp = group[group['version'] == 'OMP']
                if not carts.empty and not omp.empty:
                    carts_mean = compute_timing_stats(carts)['mean'].values[0]
                    omp_mean = compute_timing_stats(omp)['mean'].values[0]
                    if carts_mean and omp_mean and carts_mean > 0:
                        speedup = omp_mean / carts_mean
                        x_label = default_plot_template.profiling_x_label(carts.iloc[0])
                        speedup_rows.append({
                            'Example': example,
                            'Problem Size': problem_size,
                            'Threads': threads,
                            'x_label': x_label,
                            'Speedup (OMP/CARTS)': speedup
                        })
            speedup_df = pd.DataFrame(speedup_rows)
            if not speedup_df.empty:
                speedup_bar_fig = go.Figure()
                for (example,), ex_df in speedup_df.groupby(['Example']):
                    legend_name = f"{example}"
                    speedup_bar_fig.add_trace(go.Scatter(
                        x=ex_df['x_label'],
                        y=ex_df['Speedup (OMP/CARTS)'],
                        mode='lines+markers',
                        name=legend_name,
                        hovertemplate='<b>%{x}</b><br>Speedup: %{y:.4f}<br>Example: '+legend_name+'<extra></extra>'
                    ))
                speedup_bar_fig = default_plot_template.apply(
                    speedup_bar_fig,
                    xaxis_title='Threads | Size',
                    yaxis_title='Speedup (OMP/CARTS)',
                    plot_title='Speedup'
                )
        # --- Variability plot (box plot) --- #
        variability_fig = empty_fig
        if not baseline_df.empty:
            variability_fig = go.Figure()
            for (example, version), group in baseline_df.groupby(['example_name', 'version']):
                group = group.copy()
                group['x_label'] = group.apply(lambda row: default_plot_template.profiling_x_label(row), axis=1)
                legend_name = f"{example} - {version}"
                all_times = []
                all_x_labels = []
                for _, row in group.iterrows():
                    times = row['times_seconds']
                    if isinstance(times, str):
                        try:
                            times = [float(x) for x in times.split(",") if x.strip()]
                        except Exception:
                            times = []
                    elif isinstance(times, (float, int)):
                        times = [float(times)]
                    elif not isinstance(times, list):
                        times = []
                    all_times.extend(times)
                    all_x_labels.extend([row['x_label']] * len(times))
                variability_fig.add_trace(go.Box(
                    y=all_times,
                    x=all_x_labels,
                    name=legend_name,
                    boxmean=True,
                    marker=dict(color=PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR),
                    line=dict(width=2, color=PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR),
                    boxpoints='outliers',
                    showlegend=True,
                    hovertemplate='<b>%{x}</b><br>Time: %{y:.4f} s<br>Example: '+example+'<br>Version: '+version+'<extra></extra>'
                ))
            variability_fig = default_plot_template.apply(
                variability_fig,
                xaxis_title='Threads | Size',
                yaxis_title='Execution Time (s)',
                plot_title='Variability'
            )
        # --- Strong Scaling plot --- #
        strong_scaling_eff_fig = empty_fig
        if not baseline_df.empty:
            strong_scaling_eff_fig = go.Figure()
            for (example, problem_size, version), group in baseline_df.groupby(['example_name', 'problem_size', 'version']):
                group = group.copy()
                stats = compute_timing_stats(group, group_cols=['threads'])
                legend_name = f"{example} - S={problem_size} - {version}"
                if 1 in stats['threads'].values:
                    t1 = stats[stats['threads'] == 1]['mean'].values[0]
                    if t1 and t1 > 0:
                        efficiency = stats['mean'] / t1
                        strong_scaling_eff_fig.add_trace(go.Scatter(
                            x=stats['threads'],
                            y=efficiency,
                            mode='lines+markers',
                            name=legend_name,
                            hovertemplate='<b>Threads: %{x}</b><br>Efficiency: %{y:.4f}<br>Example: '+example+'<br>Size: '+str(problem_size)+'<br>Version: '+version+'<extra></extra>'
                        ))
            strong_scaling_eff_fig = default_plot_template.apply(
                strong_scaling_eff_fig,
                xaxis_title='Threads',
                yaxis_title='Efficiency (T(1)/T(N))',
                plot_title='Strong Scaling'
            )
        # Always return a list of four figures in the order: scaling, strong scaling (eff), speedup, variability
        figs = [scaling_fig, strong_scaling_eff_fig, speedup_bar_fig, variability_fig]
        # --- Table --- #
        if not baseline_df.empty:
            stats_df = compute_timing_stats(baseline_df, group_cols=['example_name', 'problem_size', 'threads', 'version'])
            # Merge in iterations and correctness from the original df
            def get_first_val(group, col):
                vals = group[col].dropna().unique()
                return vals[0] if len(vals) > 0 else None
            merged = []
            for _, row in stats_df.iterrows():
                mask = (
                    (baseline_df['example_name'] == row['example_name']) &
                    (baseline_df['problem_size'] == row['problem_size']) &
                    (baseline_df['threads'] == row['threads']) &
                    (baseline_df['version'] == row['version'])
                )
                group = baseline_df[mask]
                merged.append({
                    'Example': row['example_name'],
                    'Size': format_number(row['problem_size']),
                    'Threads': format_number(row['threads']),
                    'Version': row['version'],
                    'Mean (s)': format_number(row['mean']),
                    'Median (s)': format_number(row['median']),
                    'Min (s)': format_number(row['min']),
                    'Max (s)': format_number(row['max']),
                    'Stdev (s)': format_number(row['stdev']),
                    'Iterations': format_number(get_first_val(group, 'iterations_ran') or get_first_val(group, 'iterations_requested')),
                    'Correctness': get_first_val(group, 'correctness_status'),
                })
            summary_columns = [
                {"name": "Example", "id": "Example"},
                {"name": "Size", "id": "Size"},
                {"name": "Threads", "id": "Threads"},
                {"name": "Version", "id": "Version"},
                {"name": "Mean (s)", "id": "Mean (s)"},
                {"name": "Median (s)", "id": "Median (s)"},
                {"name": "Min (s)", "id": "Min (s)"},
                {"name": "Max (s)", "id": "Max (s)"},
                {"name": "Stdev (s)", "id": "Stdev (s)"},
                {"name": "Iterations", "id": "Iterations"},
                {"name": "Correctness", "id": "Correctness"},
            ]
            table = dash_table.DataTable(
                data=merged,
                columns=summary_columns,
                page_size=10,
                style_table={'overflowX': 'auto', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(44,111,187,0.10)'},
                style_cell={
                    'fontSize': '1.08rem', 'padding': '8px 4px', 'minWidth': '60px',
                },
                style_cell_conditional=[
                    {'if': {'column_id': c}, 'textAlign': 'right'} for c in ['Size', 'Threads', 'Mean (s)', 'Median (s)', 'Min (s)', 'Max (s)', 'Stdev (s)', 'Iterations']
                ] + [
                    {'if': {'column_id': c}, 'textAlign': 'center'} for c in ['Example', 'Version', 'Correctness']
                ],
                style_header={'backgroundColor': '#e6f0fa', 'fontWeight': 'bold', 'fontSize': '1.09rem', 'color': PRIMARY_COLOR, 'borderBottom': f'2px solid {PRIMARY_COLOR}'},
                style_data_conditional=[
                    {'if': {'column_id': 'Correctness', 'filter_query': '{Correctness} = CORRECT'}, 'backgroundColor': '#e6fae6', 'color': '#1a7f37'},
                    {'if': {'column_id': 'Correctness', 'filter_query': '{Correctness} != CORRECT'}, 'backgroundColor': '#fff3e6', 'color': '#b26a00'},
                ],
                style_as_list_view=True,
            )
        else:
            table = html.Div("No benchmarking data to display.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
    # --- Derived Metrics Table ---
    # Compose the results section
    results_section = html.Div([
        table
    ])
    # --- Tabs for main plots --- #
    tabs = dcc.Tabs(
        id='main-plot-tabs',
        value='scaling',
        children=[
            dcc.Tab(label="Scaling", value='scaling', children=[dcc.Graph(id='scaling-fig', figure=figs[0], config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})]),
            dcc.Tab(label="Strong Scaling", value='strong_scaling', children=[dcc.Graph(id='strong-scaling-fig', figure=figs[1], config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})]),
            dcc.Tab(label="Speedup", value='speedup', children=[dcc.Graph(id='speedup-fig', figure=figs[2], config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})]),
            dcc.Tab(label="Variability", value='variability', children=[dcc.Graph(id='variability-fig', figure=figs[3], config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})]),
        ],
        style={'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '8px 8px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'marginBottom': '18px', 'fontSize': '1.01em'}
    )
    return tabs, results_section

# --- Callback to update plot layout on tab change --- #
@app.callback(
    [Output('scaling-fig', 'figure'),
     Output('strong-scaling-fig', 'figure'),
     Output('speedup-fig', 'figure'),
     Output('variability-fig', 'figure')],
    [Input('main-plot-tabs', 'value')],
    [State('scaling-fig', 'figure'),
     State('strong-scaling-fig', 'figure'),
     State('speedup-fig', 'figure'),
     State('variability-fig', 'figure')]
)
def update_main_plot_layout_on_tab(tab, scaling_fig, strong_scaling_fig, speedup_fig, variability_fig):
    # Re-apply the template to the active tab's figure to update margin/legend, preserving titles/labels
    figs = [scaling_fig, strong_scaling_fig, speedup_fig, variability_fig]
    ids = ['scaling', 'strong_scaling', 'speedup', 'variability']
    out = []
    for i, (fig, tab_id) in enumerate(zip(figs, ids)):
        if tab == tab_id and fig is not None:
            go_fig = go.Figure(fig)
            # Extract current titles/labels
            plot_title = go_fig.layout.title.text if go_fig.layout.title and go_fig.layout.title.text else None
            xaxis_title = go_fig.layout.xaxis.title.text if go_fig.layout.xaxis and go_fig.layout.xaxis.title and go_fig.layout.xaxis.title.text else None
            yaxis_title = go_fig.layout.yaxis.title.text if go_fig.layout.yaxis and go_fig.layout.yaxis.title and go_fig.layout.yaxis.title.text else None
            legend_title = go_fig.layout.legend.title.text if go_fig.layout.legend and go_fig.layout.legend.title and go_fig.layout.legend.title.text else None
            # Always set height and margin to template values
            go_fig = default_plot_template.apply(go_fig, xaxis_title=xaxis_title, yaxis_title=yaxis_title, plot_title=plot_title, legend_title=legend_title)
            out.append(go_fig)
        else:
            out.append(fig)
    return out

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

# --- Add callbacks for Select All / Clear All toggles for each checklist --- #
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

# --- Split callbacks for filter modal --- #
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

# --- Optimized average per-example bar plot --- #
def average_per_example_figure(df):
    if df is None or df.empty:
        fig = go.Figure()
        fig.add_annotation(text="No data to display.", xref="paper", yref="paper", showarrow=False, 
                          font=dict(size=18, color=NEUTRAL_COLOR, family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif'))
        return default_plot_template.apply(fig)
        
    # Use vectorized groupby/agg
    carts_col = next((c for c in df.columns if c.lower().startswith('carts') and 'avg_seconds' in c), None)
    omp_col = next((c for c in df.columns if c.lower().startswith('omp') and 'avg_seconds' in c), None)
    
    if not carts_col or not omp_col:
        fig = go.Figure()
        fig.add_annotation(text="No timing data to display.", xref="paper", yref="paper", showarrow=False, 
                          font=dict(size=16, color=NEUTRAL_COLOR, family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif'))
        return default_plot_template.apply(fig)
        
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

# --- Optimized callback for average-per-example bar --- #
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

def extract_profiling_stats(profiling_results):
    rows = []
    for i, row in enumerate(profiling_results):
        for key in row.keys():
            if key.startswith("event_stats.") and key.endswith(".all"):
                event = key[len("event_stats."):-len(".all")]
                values = row[key]
                if isinstance(values, list):
                    vals = values
                elif isinstance(values, str):
                    try:
                        # Try to parse as Python list
                        vals = ast.literal_eval(values)
                        if not isinstance(vals, list):
                            raise Exception
                    except Exception:
                        # Fallback: parse as comma-separated string
                        try:
                            vals = [float(x) for x in values.split(",") if x.strip()]
                        except Exception:
                            vals = []
                else:
                    vals = []
                if isinstance(vals, list) and vals:
                    arr = np.array(vals)
                    rows.append({
                        "Example": row.get("example_name"),
                        "Problem Size": row.get("problem_size"),
                        "Threads": row.get("threads"),
                        "Version": row.get("version"),
                        "Event": event.replace(":u", ""),
                        "Min": float(np.min(arr)),
                        "Max": float(np.max(arr)),
                        "Avg": float(np.mean(arr)),
                        "Median": float(np.median(arr)),
                        "Stdev": float(np.std(arr) if len(arr) > 1 else 0.0)
                    })
    df = pd.DataFrame(rows)
    return df

# --- Update profiling-summary-section callback --- #
@app.callback(
    Output('profiling-summary-section', 'children'),
    [Input('results-store', 'data'), Input('filters-store', 'data')]
)
def update_profiling_summary_section(data, filters):
    if not data:
        return None
    df = pd.DataFrame(data)
    df = flatten_lists_in_df(df)
    dff = apply_dashboard_filters(df, filters)
    if 'run_type' in dff.columns:
        profiling_results = dff[dff['run_type'].astype(str).str.strip().str.lower() == 'profile'].to_dict('records')
    else:
        profiling_results = []
    if not profiling_results:
        return None
    df_stats = extract_profiling_stats(profiling_results)
    if df_stats.empty:
        return None
    # Format and rename columns
    formatted = []
    for _, row in df_stats.iterrows():
        formatted.append({
            'Example': row['Example'],
            'Size': format_number(row['Problem Size']),
            'Threads': format_number(row['Threads']),
            'Version': row['Version'],
            'Event': row['Event'],
            'Mean': format_number(row['Avg']),
            'Median': format_number(row['Median']),
            'Min': format_number(row['Min']),
            'Max': format_number(row['Max']),
            'Stdev': format_number(row['Stdev']),
        })
    summary_columns = [
        {"name": "Example", "id": "Example"},
        {"name": "Size", "id": "Size"},
        {"name": "Threads", "id": "Threads"},
        {"name": "Version", "id": "Version"},
        {"name": "Event", "id": "Event"},
        {"name": "Mean", "id": "Mean"},
        {"name": "Median", "id": "Median"},
        {"name": "Min", "id": "Min"},
        {"name": "Max", "id": "Max"},
        {"name": "Stdev", "id": "Stdev"},
    ]
    table = dash_table.DataTable(
        data=formatted,
        columns=summary_columns,
        page_size=10,
        style_table={'overflowX': 'auto', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(44,111,187,0.10)'},
        style_cell={
            'fontSize': '1.08rem', 'padding': '8px 4px', 'minWidth': '60px',
        },
        style_cell_conditional=[
            {'if': {'column_id': c}, 'textAlign': 'right'} for c in ['Size', 'Threads', 'Mean', 'Median', 'Min', 'Max', 'Stdev']
        ] + [
            {'if': {'column_id': c}, 'textAlign': 'center'} for c in ['Example', 'Version', 'Event']
        ],
        style_header={'backgroundColor': '#e6f0fa', 'fontWeight': 'bold', 'fontSize': '1.09rem', 'color': PRIMARY_COLOR, 'borderBottom': f'2px solid {PRIMARY_COLOR}'},
        style_data_conditional=[
            {'if': {'column_id': 'Stdev', 'filter_query': '{Stdev} >= 0.1'}, 'backgroundColor': '#fff3e6', 'color': '#b26a00'},
            {'if': {'column_id': 'Stdev', 'filter_query': '{Stdev} < 0.1'}, 'backgroundColor': '#e6fae6', 'color': '#1a7f37'},
        ],
        style_as_list_view=True,
    )
    return html.Div([
        html.H3("Profiling Event Summary", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '18px', 'marginBottom': '10px'}),
        html.Button("Download Profiling Summary CSV", id="download-profiling-btn", n_clicks=0, style={
                'backgroundColor': ACCENT_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '7px',
                'padding': '8px 18px', 'fontWeight': 'bold', 'fontSize': '1.01rem', 'marginBottom': '10px'}),
        dcc.Download(id="download-profiling-csv"),
        table])

def profiling_event_bar_chart(df, event, version):
    dff = df[(df['Event'] == event) & (df['Version'] == version.upper())]
    # Robust check: if dff is empty or missing required columns, return empty fig
    if dff.empty or not all(col in dff.columns for col in ['Example', 'Avg', 'Threads']):
        fig = go.Figure()
        fig.add_annotation(text="No data for this event/version.", xref="paper", yref="paper", showarrow=False)
        return default_plot_template.apply(fig, plot_title=f'{event} (avg) - {version.upper()}')
    # Ensure Threads has at least one non-null value and is string type
    if dff['Threads'].isnull().all():
        fig = go.Figure()
        fig.add_annotation(text="No valid thread data for this event/version.", xref="paper", yref="paper", showarrow=False)
        return default_plot_template.apply(fig, plot_title=f'{event} (avg) - {version.upper()}')
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
            plot_title=f'{event} (avg) - {version.upper()}'
        )
    except Exception as e:
        fig = go.Figure()
        fig.add_annotation(text=f"Plot error: {str(e)}", xref="paper", yref="paper", showarrow=False, font=dict(color="red"))
        fig = default_plot_template.apply(fig, plot_title=f'{event} (avg) - {version.upper()}')
    return fig

def format_number(val, ndigits=4):
    if val is None or (isinstance(val, float) and (np.isnan(val) or np.isinf(val))):
        return "-"
    if isinstance(val, int):
        return f"{val:,}"
    try:
        return f"{val:,.{ndigits}f}"
    except Exception:
        return str(val)

def compute_timing_stats(df, group_cols=["example_name", "problem_size", "threads", "version", "run_type"]):
    """Aggregate timing stats (mean, min, max, stdev, median) from times_seconds."""
    def to_float_list(val):
        # Accepts a list of floats, a comma-separated string, or a single float
        if isinstance(val, list):
            return [float(x) for x in val if x is not None]
        if isinstance(val, str):
            # Try to split comma-separated string
            try:
                return [float(x) for x in val.split(",") if x.strip()]
            except Exception:
                try:
                    return [float(val)]
                except Exception:
                    return []
        if isinstance(val, (float, int)):
            return [float(val)]
        return []
    def agg_times(x):
        # x is a Series of lists, strings, or floats
        all_vals = []
        for v in x:
            all_vals.extend(to_float_list(v))
        return all_vals
    grouped = df.groupby(group_cols).agg({"times_seconds": agg_times}).reset_index()
    def agg_stats(times_list):
        arr = np.array([float(x) for x in times_list if x is not None])
        if len(arr) == 0:
            return {"mean": None, "min": None, "max": None, "stdev": None, "median": None}
        return {
            "mean": float(np.mean(arr)),
            "min": float(np.min(arr)),
            "max": float(np.max(arr)),
            "stdev": float(np.std(arr) if len(arr) > 1 else 0.0),
            "median": float(np.median(arr)),
        }
    stats = grouped["times_seconds"].apply(agg_stats)
    for stat in ["mean", "min", "max", "stdev", "median"]:
        grouped[stat] = stats.apply(lambda d: d[stat])
    return grouped

@app.callback(
    Output('perf-event-analysis-section', 'children'),
    [Input('results-store', 'data'),
     Input('profiling-group-dropdown', 'value'),
     Input('filters-store', 'data')],
)
def update_perf_event_analysis_section(data, selected_group, filters):
    if not data:
        return html.Div("No profiling data available.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
    df = pd.DataFrame(data)
    df = flatten_lists_in_df(df)
    dff = apply_dashboard_filters(df, filters)
    if 'run_type' in dff.columns:
        profiling_results = dff[dff['run_type'].astype(str).str.strip().str.lower() == 'profile'].to_dict('records')
    else:
        profiling_results = []
    if not profiling_results:
        return html.Div("No profiling data available.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
    df_stats = extract_profiling_stats(profiling_results)
    if df_stats.empty:
        return html.Div("No profiling data available.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
    # Use the same event group mapping as in the dropdown callback
    event_group_map = {
        'L1': ['L1-dcache', 'L1-icache'],
        'L2': ['L2-dcache', 'l2_rqsts'],
        'L3': ['L3-dcache'],
        'TLB': ['TLB', 'dTLB', 'iTLB'],
        'Stalls': ['stalls', 'resource_stalls', 'cycle_activity'],
        'Clocks/Freq': ['cycles', 'cpu-clock', 'task-clock'],
        'Instructions': ['instructions'],
        'Memory': ['mem_inst_retired'],
        'Cache': ['cache-references', 'cache-misses'],
    }
    event_to_group = {}
    for event in df_stats['Event'].unique():
        found = False
        for group, substrings in event_group_map.items():
            if any(sub in event for sub in substrings):
                event_to_group[event] = group
                found = True
                break
        if not found:
            event_to_group[event] = 'Other'
    # Filter events by selected group using the mapping
    if selected_group:
        filtered_events = sorted([e for e in df_stats['Event'].unique() if event_to_group.get(e) == selected_group])
    else:
        filtered_events = sorted(df_stats['Event'].unique())
    event_tabs = []
    for event in filtered_events:
        fig = go.Figure()
        for (example, version), dff in df_stats[df_stats['Event'] == event].groupby(['Example', 'Version']):
            dff = dff.copy()
            dff['x_label'] = dff.apply(lambda row: default_plot_template.profiling_x_label(row), axis=1)
            legend_name = f"{example} - {version}"
            fig.add_trace(go.Scatter(
                x=dff['x_label'],
                y=dff['Avg'],
                mode='lines+markers',
                name=legend_name,
                line=dict(width=2, color=PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR),
                marker=dict(size=10, line=dict(width=1, color='black')),
                customdata=dff[['Example', 'Threads', 'Problem Size']].values,
                hovertemplate='<b>%{customdata[0]}</b><br>Threads: %{customdata[1]}<br>Size: %{customdata[2]}<br>Avg: %{y:,.4f}<extra></extra>'
            ))
        fig = default_plot_template.apply(
            fig,
            xaxis_title='Threads | Size' if 'Problem Size' in dff.columns else 'Threads',
            yaxis_title=event,
            plot_title=f'{event} (avg)'
        )
        event_tabs.append(dcc.Tab(label=event, children=[
            dcc.Graph(figure=fig, config={'displayModeBar': True, 'responsive': False}, style={'height': '600px'})
        ]))
    if not event_tabs:
        return html.Div("No event data to display for this group.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
    return html.Div([
        dcc.Tabs(children=event_tabs, style={'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '8px 8px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'marginBottom': '18px', 'fontSize': '1.01em'})
    ], style={'backgroundColor': '#f9fafc', 'borderRadius': '8px', 'padding': '18px 18px', 'marginBottom': '18px'})

# --- Populate profiling-group-dropdown options based on event groups in results --- #
@app.callback(
    Output('profiling-group-dropdown', 'options'),
    [Input('results-store', 'data')]
)
def update_profiling_group_dropdown_options(data):
    if not data:
        return []
    profiling_results = [row for row in data if row.get('run_type') == 'profile']
    if not profiling_results:
        return []
    df = extract_profiling_stats(profiling_results)
    if df.empty or 'Event' not in df:
        return []
    # Define static event group mapping
    event_group_map = {
        'L1': ['L1-dcache', 'L1-icache'],
        'L2': ['L2-dcache', 'l2_rqsts'],
        'L3': ['L3-dcache'],
        'TLB': ['TLB', 'dTLB', 'iTLB'],
        'Stalls': ['stalls', 'resource_stalls', 'cycle_activity'],
        'Clocks/Freq': ['cycles', 'cpu-clock', 'task-clock'],
        'Instructions': ['instructions'],
        'Memory': ['mem_inst_retired'],
        'Cache': ['cache-references', 'cache-misses'],
    }
    event_to_group = {}
    for event in df['Event'].unique():
        found = False
        for group, substrings in event_group_map.items():
            if any(sub in event for sub in substrings):
                event_to_group[event] = group
                found = True
                break
        if not found:
            event_to_group[event] = 'Other'
    group_set = sorted(set(event_to_group.values()))
    return [{'label': g, 'value': g} for g in group_set]

# --- Set initial value of profiling-group-dropdown to 'Cache' if present, or first group --- #
@app.callback(
    Output('profiling-group-dropdown', 'value'),
    [Input('profiling-group-dropdown', 'options')],
    prevent_initial_call=True
)
def set_initial_profiling_group_value(options):
    if not options:
        return None
    group_labels = [opt['value'] for opt in options]
    if 'Cache' in group_labels:
        return 'Cache'
    return group_labels[0] if group_labels else None

# --- CSV Download Callbacks --- #
@app.callback(
    Output("download-benchmark-csv", "data"),
    [Input("download-benchmark-btn", "n_clicks")],
    [State('filters-store', 'data'), State('results-store', 'data')],
    prevent_initial_call=True,
)
def download_benchmark_csv(n_clicks, filters, data):
    if not data or not n_clicks:
        raise PreventUpdate
    df = pd.DataFrame(data)
    df = flatten_lists_in_df(df)
    dff = apply_dashboard_filters(df, filters)
    # Compute the same summary table as in the UI
    stats_df = compute_timing_stats(dff[dff['run_type'] == 'no_profile'], group_cols=['example_name', 'problem_size', 'threads', 'version'])
    if stats_df.empty:
        raise PreventUpdate
    # Format columns as in the UI
    def get_first_val(group, col):
        vals = group[col].dropna().unique()
        return vals[0] if len(vals) > 0 else None
    merged = []
    baseline_df = dff[dff['run_type'] == 'no_profile']
    for _, row in stats_df.iterrows():
        mask = (
            (baseline_df['example_name'] == row['example_name']) &
            (baseline_df['problem_size'] == row['problem_size']) &
            (baseline_df['threads'] == row['threads']) &
            (baseline_df['version'] == row['version'])
        )
        group = baseline_df[mask]
        merged.append({
            'Example': row['example_name'],
            'Size': format_number(row['problem_size']),
            'Threads': format_number(row['threads']),
            'Version': row['version'],
            'Mean (s)': format_number(row['mean']),
            'Median (s)': format_number(row['median']),
            'Min (s)': format_number(row['min']),
            'Max (s)': format_number(row['max']),
            'Stdev (s)': format_number(row['stdev']),
            'Iterations': format_number(get_first_val(group, 'iterations_ran') or get_first_val(group, 'iterations_requested')),
            'Correctness': get_first_val(group, 'correctness_status'),
        })
    out_df = pd.DataFrame(merged)
    csv_string = out_df.to_csv(index=False)
    return dict(content=csv_string, filename="benchmark_summary.csv")

@app.callback(
    Output("download-profiling-csv", "data"),
    [Input("download-profiling-btn", "n_clicks")],
    [State('filters-store', 'data'), State('results-store', 'data')],
    prevent_initial_call=True,
)
def download_profiling_csv(n_clicks, filters, data):
    if not data or not n_clicks:
        raise PreventUpdate
    df = pd.DataFrame(data)
    df = flatten_lists_in_df(df)
    dff = apply_dashboard_filters(df, filters)
    if 'run_type' in dff.columns:
        profiling_results = dff[dff['run_type'].astype(str).str.strip().str.lower() == 'profile'].to_dict('records')
    else:
        profiling_results = []
    if not profiling_results:
        raise PreventUpdate
    df_stats = extract_profiling_stats(profiling_results)
    if df_stats.empty:
        raise PreventUpdate
    # Format columns as in the UI
    formatted = []
    for _, row in df_stats.iterrows():
        formatted.append({
            'Example': row['Example'],
            'Size': format_number(row['Problem Size']),
            'Threads': format_number(row['Threads']),
            'Version': row['Version'],
            'Event': row['Event'],
            'Mean': format_number(row['Avg']),
            'Median': format_number(row['Median']),
            'Min': format_number(row['Min']),
            'Max': format_number(row['Max']),
            'Stdev': format_number(row['Stdev']),
        })
    out_df = pd.DataFrame(formatted)
    csv_string = out_df.to_csv(index=False)
    return dict(content=csv_string, filename="profiling_summary.csv")

if __name__ == "__main__":
    app.run(debug=True)