import os
import json
import pandas as pd
import dash_bootstrap_components as dbc
import dash
from dash import dcc, html, dash_table, Input, Output, State, ctx, MATCH, ALL, ALLSMALLER, callback_context
import plotly.express as px
import plotly.graph_objs as go
from plotly.subplots import make_subplots
import shutil
import datetime
import numpy as np
from dash.exceptions import PreventUpdate
import io
import base64
import re
import traceback
import ast
from dash.dependencies import ALL


# --- File Paths ---
DEFAULT_RESULTS_PATH = os.path.join(os.path.dirname(__file__) if '__file__' in locals() else '.', 'output/performance_results.json')

# --- Styling Constants ---
PRIMARY_COLOR = '#1f77b4'
ACCENT_COLOR = '#ff7f0e'
NEUTRAL_COLOR = '#505f79'
MAIN_BG_COLOR = '#f5f9fc'
CARD_BG_COLOR = '#ffffff'
SUCCESS_COLOR = '#36b37e'
WARNING_COLOR = '#ffab00'
PLOT_BG_COLOR = 'rgb(229, 236, 246)'
GRID_COLOR = '#cccccc'
MARKER_OUTLINE_COLOR = 'black'

# --- PlotTemplate class ---
class PlotTemplate:
    """
    A template class to apply consistent styling and layout to Plotly figures.
    It defines default visual properties for plots, which can be overridden.
    """
    def __init__(self,
                 legend_orientation='h', legend_y=-0.25, legend_x=0.5, # Legend at bottom-center
                 height=600, margin=None, bargap=0.18,
                 grid_color=GRID_COLOR, axis_color='#666666', plot_bgcolor=PLOT_BG_COLOR, paper_bgcolor='white',
                 bar_line_width=1.5, bar_width=0.4, line_width=2, marker_line_width=1,
                 font_family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif', font_size=14,
                 xaxis_title='', yaxis_title='', plot_title='', legend_title=''):
        if margin is None:
            margin = dict(l=60, r=40, t=80, b=120)
        self.legend_orientation = legend_orientation
        self.legend_y = legend_y
        self.legend_x = legend_x
        self.height = height
        self.margin = margin
        self.bargap = bargap
        self.grid_color = grid_color
        self.axis_color = axis_color
        self.plot_bgcolor = plot_bgcolor
        self.paper_bgcolor = paper_bgcolor
        self.bar_line_width = bar_line_width
        self.bar_width = bar_width
        self.line_width = line_width
        self.marker_line_width = marker_line_width # Default for marker outlines
        self.font_family = font_family
        self.font_size = font_size
        self.xaxis_title_size = font_size + 1
        self.plot_title_size = font_size + 4
        self.xaxis_title = xaxis_title
        self.yaxis_title = yaxis_title
        self.plot_title = plot_title
        self.legend_title = legend_title

    def apply(self, fig, xaxis_title=None, yaxis_title=None, plot_title=None, legend_title=None, height=None):
        """
        Applies the predefined styles and layout options to a given Plotly figure.
        Allows overriding of titles, height, etc., on a per-plot basis.
        """
        fig_height = height if height is not None else self.height
        current_margin = self.margin.copy()

        title_text_val = plot_title if plot_title is not None else self.plot_title
        legend_title_val = legend_title if legend_title is not None else self.legend_title

        if self.legend_orientation == 'h' and self.legend_y < 0:
            current_margin['b'] = max(current_margin.get('b', 120), 120 + abs(self.legend_y * 200)) # Increased bottom margin for legend
        else:
            current_margin['b'] = max(60, current_margin.get('b', 100) - 40)

        if not title_text_val:
            current_margin['t'] = max(30, current_margin.get('t', 80) - 40)

        fig.update_layout(
            height=fig_height,
            title=dict(text=title_text_val, x=0.5, font=dict(size=self.plot_title_size, family=self.font_family)),
            font=dict(family=self.font_family, size=self.font_size),
            legend=dict(
                title=dict(text=legend_title_val, font=dict(size=self.font_size -1)),
                orientation=self.legend_orientation, yanchor='top', y=self.legend_y,
                xanchor='center', x=self.legend_x, bgcolor='rgba(255,255,255,0.85)',
                bordercolor='#cccccc', borderwidth=1, font=dict(size=self.font_size - 2)
            ),
            plot_bgcolor=self.plot_bgcolor, paper_bgcolor=self.paper_bgcolor, margin=current_margin, bargap=self.bargap,
            xaxis=dict(
                title=dict(text=xaxis_title if xaxis_title is not None else self.xaxis_title, font=dict(size=self.xaxis_title_size, color=NEUTRAL_COLOR)),
                tickfont=dict(size=self.font_size - 2), gridcolor=self.grid_color, showgrid=True,
                showline=True, linecolor=self.axis_color, ticks='outside', automargin=True,
            ),
            yaxis=dict(
                title=dict(text=yaxis_title if yaxis_title is not None else self.yaxis_title, font=dict(size=self.xaxis_title_size, color=NEUTRAL_COLOR)),
                tickfont=dict(size=self.font_size - 2), gridcolor=self.grid_color, showgrid=True,
                showline=True, linecolor=self.axis_color, ticks='outside', automargin=True,
            ),
            hoverlabel=dict(bgcolor="white", font_size=self.font_size -1, font_family=self.font_family, bordercolor="#cccccc")
        )
        # Apply styles to annotations (e.g., subplot titles)
        if fig.layout.annotations:
            fig.update_annotations(font_size=self.font_size) # Adjust as needed for subplot title size

        for trace in fig.data:
            if trace.type == 'bar':
                if not hasattr(trace.marker, 'color') or trace.marker.color is None:
                    trace.marker.color = PRIMARY_COLOR
                if not hasattr(trace.marker, 'line') or not hasattr(trace.marker.line, 'width') or trace.marker.line.width is None:
                    trace.marker.line.width = self.bar_line_width
                if not hasattr(trace.marker.line, 'color') or trace.marker.line.color is None:
                    trace.marker.line.color = trace.marker.color # Outline same as fill by default
            elif trace.type == 'scatter':
                if hasattr(trace, 'line') and (not hasattr(trace.line, 'width') or trace.line.width is None):
                    trace.line.width = self.line_width

                if hasattr(trace, 'mode') and 'markers' in trace.mode:
                    if not hasattr(trace.marker, 'size') or trace.marker.size is None:
                        trace.marker.size = 8 # Default marker size
                    if not hasattr(trace.marker, 'line') or trace.marker.line is None:
                        trace.marker.line = go.scatter.marker.Line() # Ensure line object exists
                    if not hasattr(trace.marker.line, 'width') or trace.marker.line.width is None:
                        trace.marker.line.width = self.marker_line_width
                    if not hasattr(trace.marker.line, 'color') or trace.marker.line.color is None:
                        trace.marker.line.color = MARKER_OUTLINE_COLOR # Default marker outline color
                    # trace.marker.color (fill) should be set by plotting functions
        return fig

    def profiling_x_label(self, row):
        """Generates a consistent x-axis label for profiling plots (Threads | Size)."""
        if hasattr(row, 'to_dict'): row = row.to_dict()
        threads = row.get('threads', row.get('Threads', None))
        problem_size = row.get('problem_size', row.get('Problem Size', None))
        if problem_size is not None and pd.notnull(problem_size) and str(problem_size).lower() != 'default' and str(problem_size).lower() != 'n/a':
            return f"T={threads} | S={problem_size}"
        return f"T={threads}"

# --- Instantiate a default template --- #
default_plot_template = PlotTemplate(
    yaxis_title='Value',
    legend_title='Version'
)

# --- StyleManager Class (remains largely the same, ensure font family matches template) ---
class StyleManager:
    MAIN_DIV_STYLE = {
        'background': 'linear-gradient(135deg, #f5f9fc 0%, #ffffff 100%)', 'minHeight': '100vh',
        'fontFamily': default_plot_template.font_family, 'color': NEUTRAL_COLOR,
    }
    CARD_STYLE = {
        'backgroundColor': CARD_BG_COLOR, 'borderRadius': '16px', 'boxShadow': '0 8px 24px rgba(0,0,0,0.08)',
        'padding': '24px', 'margin': '24px auto', 'maxWidth': '1400px', 'border': '1px solid #f0f0f0',
    }
    HEADER_STYLE = {
        'color': PRIMARY_COLOR, 'fontWeight': '600', 'fontSize': '2.4rem', 'marginBottom': '10px',
        'textAlign': 'center', 'letterSpacing': '0.5px', 'padding': '20px 0 15px 0',
        'borderBottom': f'2px solid {PRIMARY_COLOR}', 'background': 'transparent', 'position': 'relative',
    }
    SUBTITLE_STYLE = {
        'color': NEUTRAL_COLOR, 'fontSize': '1.2rem', 'textAlign': 'center', 'marginBottom': '24px',
        'fontWeight': '400', 'maxWidth': '900px', 'margin': '0 auto 24px auto',
    }
    LABEL_STYLE = {
        'fontWeight': '600', 'color': PRIMARY_COLOR, 'marginTop': '14px',
        'fontSize': '1.05rem', 'letterSpacing': '0.2px',
    }
    FILTER_MODAL_CLOSE_STYLE = {
        'position': 'absolute', 'top': '10px', 'right': '18px', 'background': 'none',
        'border': 'none', 'outline': 'none', 'padding': '0 12px', 'marginLeft': 'auto', 'cursor': 'pointer',
    }
    FILTER_MODAL_CONTENT_STYLE = {
        'backgroundColor': '#fff', 'borderRadius': '14px', 'boxShadow': '0 8px 32px rgba(31,119,180,0.18)',
        'padding': '14px 8px 14px 8px', 'fontFamily': default_plot_template.font_family, 'fontSize': '1.00rem',
        'width': '100%', 'maxWidth': '420px', 'marginLeft': '0', 'marginRight': '0', 'border': '1.2px solid #d0e3f7',
    }
    FILTER_MODAL_OVERLAY_STYLE = {
        'position': 'fixed', 'top': 0, 'left': 0, 'width': '100vw', 'height': '100vh',
        'background': 'rgba(0,0,0,0.18)', 'zIndex': 1000, 'display': 'flex',
        'alignItems': 'center', 'justifyContent': 'center',
    }
    SUMMARY_CARD_STYLE = {
        'backgroundColor': '#e6f0fa', 'padding': '15px', 'borderRadius': '10px',
        'marginBottom': '20px', 'boxShadow': '0 2px 4px rgba(0,0,0,0.05)'
    }
    SUMMARY_ITEM_STYLE = {'fontSize': '0.95rem', 'marginBottom': '5px', 'color': NEUTRAL_COLOR}
    SUMMARY_LABEL_STYLE = {'fontWeight': 'bold', 'color': PRIMARY_COLOR, 'marginRight': '5px'}

    TABLE_HEADER_STYLE = {
        'backgroundColor': '#e6f0fa', 'fontWeight': 'bold', 'fontSize': '1.09rem',
        'color': PRIMARY_COLOR, 'borderBottom': f'2px solid {PRIMARY_COLOR}',
        'fontFamily': default_plot_template.font_family
    }
    TABLE_CELL_STYLE = {
        'fontSize': '1.08rem', 'padding': '8px 4px', 'minWidth': '60px',
        'fontFamily': default_plot_template.font_family
    }
    HELP_MODAL_STYLE = {
        'position': 'fixed', 'zIndex': 1050, 'left': 0, 'top': 0, 'width': '100%', 'height': '100%',
        'overflow': 'auto', 'backgroundColor': 'rgba(0,0,0,0.4)'
    }
    HELP_MODAL_CONTENT_STYLE = {
        'backgroundColor': '#fefefe',
        'margin': '15% auto',
        'padding': '20px',
        'border': '1px solid #888',
        'width': '60%',
        'maxWidth': '600px',
        'borderRadius': '10px',
        'boxShadow': '0 4px 8px 0 rgba(0,0,0,0.2),0 6px 20px 0 rgba(0,0,0,0.19)',
        'fontFamily': default_plot_template.font_family,  # Ensures font family consistency
        'fontSize': '1.08rem',  # Match your main UI font size
    }
    HELP_MODAL_CLOSE_BUTTON_STYLE = {
        'color': '#aaa', 'float': 'right', 'fontSize': '28px', 'fontWeight': 'bold', 'cursor': 'pointer'
    }


# --- Utility Functions (load_results, flatten_lists_in_df, format_number) ---
def load_results(path):
    if not os.path.exists(path):
        return None, f"Results file not found: {path}"
    try:
        with open(path) as f:
            data = json.load(f)
        
        raw_results = []
        if 'benchmark_results' in data and isinstance(data['benchmark_results'], list):
            raw_results.extend(data['benchmark_results'])
        if 'profiling_results' in data and isinstance(data['profiling_results'], list):
            raw_results.extend(data['profiling_results'])

        if not raw_results:
            return None, "No results found in JSON."

        # Ensure event_stats column exists for all records
        processed_results = []
        for r in raw_results:
            # Ensure 'event_stats' is a dict for profiling, None otherwise
            if r.get('run_type', '').lower() == 'profile':
                if 'event_stats' not in r or not isinstance(r['event_stats'], dict):
                    r['event_stats'] = {} # Ensure it's a dict if it's a profile run but missing/malformed
            else:
                r['event_stats'] = None # Set to None for non-profile runs or if missing
            processed_results.append(r)
            
        df = pd.DataFrame(processed_results) # Use pd.DataFrame directly
        return df, None
    except Exception as e:
        return None, f"Error loading file: {e}"


def flatten_lists_in_df(df):
    for col in df.columns:
        if df[col].apply(type).eq(list).any():
            df[col] = df[col].apply(lambda x: ', '.join(map(str, x)) if isinstance(x, list) else x)
    return df

def format_number(val, ndigits=4):
    if val is None or (isinstance(val, float) and (np.isnan(val) or np.isinf(val))): return "-"
    if isinstance(val, (int, np.integer)): return f"{val:,}"
    try:
        float_val = float(val)
        return f"{float_val:,.{ndigits}f}"
    except (ValueError, TypeError): return str(val)


# --- DataManager Class ---
class DataManager:
    def __init__(self, data_records=None):
        self.raw_df = pd.DataFrame()
        if data_records:
            self.raw_df = pd.DataFrame(data_records)
            if not self.raw_df.empty:
                self.raw_df = self._flatten_df_internal(self.raw_df)
                self._convert_column_types()

    def _flatten_df_internal(self, df):
        for col in df.columns:
            # Only flatten if the column actually contains lists and is not 'event_stats'
            if col != 'event_stats' and df[col].apply(type).eq(list).any():
                df[col] = df[col].apply(lambda x: ', '.join(map(str, x)) if isinstance(x, list) else x)
        return df

    def _convert_column_types(self):
        if 'threads' in self.raw_df.columns:
            self.raw_df['threads'] = pd.to_numeric(self.raw_df['threads'], errors='coerce')
        if 'problem_size' in self.raw_df.columns:
            self.raw_df['problem_size'] = self.raw_df['problem_size'].astype(str)

    def _apply_filters(self, df, filters):
        dff = df.copy()
        if not filters: return dff
        
        example_filter_values = filters.get('examples')
        if example_filter_values and 'example_name' in dff.columns:
            dff = dff[dff['example_name'].isin(example_filter_values)]

        thread_filter_values = filters.get('threads')
        if thread_filter_values and 'threads' in dff.columns:
            filter_threads_numeric = [pd.to_numeric(t, errors='coerce') for t in thread_filter_values]
            filter_threads_numeric = [t for t in filter_threads_numeric if pd.notna(t)]
            if filter_threads_numeric: dff = dff[dff['threads'].isin(filter_threads_numeric)]

        size_filter_values = filters.get('sizes')
        if size_filter_values and 'problem_size' in dff.columns:
            dff = dff[dff['problem_size'].isin([str(s) for s in size_filter_values])]
        
        if filters.get('correctness') and filters['correctness'] != 'ALL' and 'correctness_status' in dff.columns:
            dff = dff[dff['correctness_status'].astype(str).str.upper() == filters['correctness']]
        return dff

    def get_filtered_data(self, filters=None):
        if self.raw_df.empty: return pd.DataFrame()
        return self._apply_filters(self.raw_df, filters)

    def get_benchmark_summary(self, filters=None):
        if self.raw_df.empty: return pd.DataFrame()
        dff = self.get_filtered_data(filters)
        return self._generate_benchmark_metrics_from_df(dff)

    def _generate_benchmark_metrics_from_df(self, df_input):
        if df_input.empty: return pd.DataFrame()
        temp_df = df_input.copy()
        if 'run_type' not in temp_df.columns: temp_df['run_type_temp'] = 'no_profile'
        else: temp_df['run_type_temp'] = temp_df['run_type']
        baseline_df = temp_df[temp_df['run_type_temp'].astype(str).str.lower() == 'no_profile'].copy()
        if 'run_type_temp' in temp_df.columns and 'run_type' not in temp_df.columns :
            baseline_df = baseline_df.drop(columns=['run_type_temp'])
        if baseline_df.empty: return pd.DataFrame()

        def process_times_robust(times_entry):
            if isinstance(times_entry, str):
                try: return [float(t.strip()) for t in times_entry.split(',') if t.strip()]
                except ValueError: return []
            elif isinstance(times_entry, list):
                return [float(t) for t in times_entry if isinstance(t, (int, float))]
            elif isinstance(times_entry, (int, float)): return [float(times_entry)]
            return []

        results = []
        required_cols = ['example_name', 'problem_size', 'threads', 'version', 'times_seconds']
        for col in required_cols:
            if col not in baseline_df.columns:
                if col == 'problem_size': baseline_df[col] = 'N/A'
                elif col == 'times_seconds': baseline_df[col] = pd.Series([[] for _ in range(len(baseline_df))])
                else: return pd.DataFrame()

        for (example, problem_size, threads, version), group in baseline_df.groupby(['example_name', 'problem_size', 'threads', 'version']):
            all_times_flat = [item for sublist in group['times_seconds'].apply(process_times_robust) for item in sublist]
            if not all_times_flat: continue
            times_arr = np.array(all_times_flat)
            results.append({
                'example_name': example, 'problem_size': problem_size, 'threads': threads, 'version': version,
                'mean_time': np.mean(times_arr), 'median_time': np.median(times_arr),
                'min_time': np.min(times_arr), 'max_time': np.max(times_arr),
                'stdev_time': np.std(times_arr) if len(times_arr) > 1 else 0,
                'iterations': len(times_arr),
                'correctness': group['correctness_status'].iloc[0] if 'correctness_status' in group.columns and not group['correctness_status'].empty else 'UNKNOWN'
            })
        summary_df = pd.DataFrame(results)
        if summary_df.empty: return summary_df

        final_summary_list = []
        for (example, problem_size), group_ex_ps in summary_df.groupby(['example_name', 'problem_size']):
            t1_carts = group_ex_ps[(group_ex_ps['threads'] == 1) & (group_ex_ps['version'] == 'CARTS')]['mean_time'].values
            t1_omp = group_ex_ps[(group_ex_ps['threads'] == 1) & (group_ex_ps['version'] == 'OMP')]['mean_time'].values
            t1_carts_val = t1_carts[0] if len(t1_carts) > 0 else None
            t1_omp_val = t1_omp[0] if len(t1_omp) > 0 else None

            for _, row_series in group_ex_ps.iterrows():
                row_dict = row_series.to_dict()
                if row_dict['version'] == 'CARTS':
                    omp_time_series = group_ex_ps[(group_ex_ps['threads'] == row_dict['threads']) & (group_ex_ps['version'] == 'OMP')]['mean_time']
                    if len(omp_time_series) > 0 and omp_time_series.iloc[0] is not None and row_dict['mean_time'] > 0:
                        row_dict['speedup_vs_other'] = omp_time_series.iloc[0] / row_dict['mean_time']
                        row_dict['omp_time_for_comparison'] = omp_time_series.iloc[0]
                elif row_dict['version'] == 'OMP':
                    carts_time_series = group_ex_ps[(group_ex_ps['threads'] == row_dict['threads']) & (group_ex_ps['version'] == 'CARTS')]['mean_time']
                    if len(carts_time_series) > 0 and carts_time_series.iloc[0] is not None:
                        row_dict['carts_time_for_comparison'] = carts_time_series.iloc[0]
                if row_dict['threads'] > 1:
                    t1_baseline = t1_carts_val if row_dict['version'] == 'CARTS' else t1_omp_val
                    if t1_baseline is not None and row_dict['mean_time'] > 0:
                        row_dict['parallel_efficiency'] = t1_baseline / (row_dict['threads'] * row_dict['mean_time'])
                final_summary_list.append(row_dict)
        return pd.DataFrame(final_summary_list) if final_summary_list else pd.DataFrame()


    def get_profiling_data(self, filters=None):
        if self.raw_df.empty: return pd.DataFrame()
        dff = self.get_filtered_data(filters)
        if 'event_stats' not in dff.columns: return pd.DataFrame()
        processed_profile_df = self._process_raw_profiling_data(dff)
        if processed_profile_df.empty: return pd.DataFrame()
        categorized_df = self._categorize_profiling_events(processed_profile_df)
        return categorized_df

    def _process_raw_profiling_data(self, df_input):
        if df_input.empty: return pd.DataFrame()
        if 'event_stats' not in df_input.columns: return pd.DataFrame()

        temp_df = df_input.copy()
        if 'run_type' not in temp_df.columns: temp_df['run_type_temp'] = 'profile'
        else: temp_df['run_type_temp'] = temp_df['run_type']

        profile_df = temp_df[(temp_df['run_type_temp'].astype(str).str.lower() == 'profile') & temp_df['event_stats'].notna()].copy()

        if 'run_type_temp' in temp_df.columns and 'run_type' not in temp_df.columns:
            profile_df = profile_df.drop(columns=['run_type_temp'])
        if profile_df.empty: return pd.DataFrame()

        results = []
        for _, row_series in profile_df.iterrows():
            row = row_series.to_dict()
            event_stats_dict = row.get('event_stats')
            if not isinstance(event_stats_dict, dict): continue

            for event_name_raw, event_data_container in event_stats_dict.items():
                event_name = event_name_raw.replace(":u", "")
                values_list_or_str = None
                if isinstance(event_data_container, dict):
                    values_list_or_str = event_data_container.get('all')

                current_values = []
                if isinstance(values_list_or_str, str):
                    try: current_values = [float(v.strip()) for v in values_list_or_str.split(',') if v.strip()]
                    except ValueError: pass
                elif isinstance(values_list_or_str, list):
                    current_values = [float(v) for v in values_list_or_str if isinstance(v, (int, float))]

                if not current_values: continue

                value_array = np.array(current_values)
                mean_val = np.mean(value_array)
                results.append({
                    'example_name': row.get('example_name'), 'problem_size': row.get('problem_size'),
                    'threads': row.get('threads'), 'version': row.get('version'), 'event': event_name,
                    'mean': mean_val, 'median': np.median(value_array), 'min': np.min(value_array),
                    'max': np.max(value_array), 'stdev': np.std(value_array) if len(value_array) > 1 else 0,
                    'cov': (np.std(value_array) / mean_val) if mean_val != 0 and len(value_array) > 1 else 0
                })
        return pd.DataFrame(results)

    def _categorize_profiling_events(self, processed_profile_df):
        if processed_profile_df.empty: return processed_profile_df
        result_df = processed_profile_df.copy()
        categories = {
            'Cache': ['cache-references', 'cache-misses', 'L1-dcache', 'L1-icache', 'l2_rqsts', 'LLC-loads', 'LLC-load-misses', 'LLC-stores', 'LLC-store-misses'],
            'Memory': ['mem_inst_retired', 'mem-loads', 'mem-stores', 'cycle_activity.stalls_mem'],
            'CPU': ['cycles', 'instructions', 'cpu-clock', 'task-clock', 'branch-instructions', 'branch-misses'],
            'TLB': ['dTLB', 'iTLB', 'dTLB-load-misses', 'iTLB-load-misses'],
            'Stalls': ['stalls', 'resource_stalls', 'cycle_activity.stalls_total', 'cycle_activity.stalls_l1d_miss', 'cycle_activity.stalls_l2_miss', 'cycle_activity.stalls_l3_miss']
        }
        result_df['category'] = 'Other'
        for category, patterns in categories.items():
            for pattern in patterns:
                result_df.loc[result_df['event'].astype(str).str.contains(pattern, case=False, na=False), 'category'] = category

        derived_metrics = []
        if not result_df.empty and all(col in result_df.columns for col in ['example_name', 'problem_size', 'threads', 'version', 'event', 'mean']):
            for (ex, ps, th, ver), group in result_df.groupby(['example_name', 'problem_size', 'threads', 'version']):
                refs_series = group[group['event'] == 'cache-references']['mean']
                misses_series = group[group['event'] == 'cache-misses']['mean']
                if not refs_series.empty and not misses_series.empty and refs_series.iloc[0] > 0:
                    miss_rate = misses_series.iloc[0] / refs_series.iloc[0]
                    derived_metrics.append({'example_name': ex, 'problem_size': ps, 'threads': th, 'version': ver, 'event': 'cache-miss-rate', 'mean': miss_rate, 'category': 'Cache'})

                instr_series = group[group['event'] == 'instructions']['mean']
                cycles_series = group[group['event'] == 'cycles']['mean']
                if not instr_series.empty and not cycles_series.empty and cycles_series.iloc[0] > 0:
                    ipc = instr_series.iloc[0] / cycles_series.iloc[0]
                    derived_metrics.append({'example_name': ex, 'problem_size': ps, 'threads': th, 'version': ver, 'event': 'IPC', 'mean': ipc, 'category': 'CPU'})
            if derived_metrics:
                result_df = pd.concat([result_df, pd.DataFrame(derived_metrics)], ignore_index=True)
        return result_df

    def get_system_info(self):
        if self.raw_df.empty or 'system_information' not in self.raw_df.columns:
            return None
        for info in self.raw_df['system_information']:
            if isinstance(info, dict): return info
        return None

    def get_overall_summary_stats(self, filters=None):
        if self.raw_df.empty:
            return {"total_examples": 0, "total_problem_sizes": 0, "total_threads": 0,
                    "correctness_counts": {}, "carts_wins": 0, "omp_wins": 0, "ties": 0}

        dff = self.get_filtered_data(filters)
        if dff.empty:
            return {"total_examples": 0, "total_problem_sizes": 0, "total_threads": 0,
                    "correctness_counts": {}, "carts_wins": 0, "omp_wins": 0, "ties": 0}

        summary_stats = {
            "total_examples": dff['example_name'].nunique() if 'example_name' in dff.columns else 0,
            "total_problem_sizes": dff['problem_size'].nunique() if 'problem_size' in dff.columns else 0,
            "total_threads": dff['threads'].nunique() if 'threads' in dff.columns else 0,
            "correctness_counts": dff['correctness_status'].value_counts().to_dict() if 'correctness_status' in dff.columns else {},
            "carts_wins": 0, "omp_wins": 0, "ties": 0
        }

        benchmark_summary_df = self._generate_benchmark_metrics_from_df(dff)
        if not benchmark_summary_df.empty and 'mean_time' in benchmark_summary_df.columns:
            for _, group in benchmark_summary_df.groupby(['example_name', 'problem_size', 'threads']):
                carts_run = group[group['version'] == 'CARTS']
                omp_run = group[group['version'] == 'OMP']
                if not carts_run.empty and not omp_run.empty:
                    carts_time = carts_run['mean_time'].iloc[0]
                    omp_time = omp_run['mean_time'].iloc[0]
                    if pd.notna(carts_time) and pd.notna(omp_time):
                        if carts_time < omp_time: summary_stats["carts_wins"] += 1
                        elif omp_time < carts_time: summary_stats["omp_wins"] += 1
                        else: summary_stats["ties"] += 1
        return summary_stats


# --- PlotManager Class ---
class PlotManager:
    def __init__(self, plot_template, data_manager):
        self.template = plot_template
        self.dm = data_manager

    # get_average_per_example_plot method removed

    def get_scaling_plot(self, filters=None):
        summary_df = self.dm.get_benchmark_summary(filters)
        return create_scaling_plot(summary_df, self.template)

    def get_efficiency_plot(self, filters=None):
        summary_df = self.dm.get_benchmark_summary(filters)
        return create_parallel_efficiency_plot(summary_df, self.template)

    def get_speedup_plot(self, filters=None):
        summary_df = self.dm.get_benchmark_summary(filters)
        return create_speedup_comparison_plot(summary_df, self.template)

    def get_slowdown_plot(self, filters=None):
        summary_df = self.dm.get_benchmark_summary(filters)
        return create_slowdown_comparison_plot(summary_df, self.template)

    def get_variability_plot_components(self, filters=None): # Renamed
        dff_for_variability = self.dm.get_filtered_data(filters)
        return create_execution_time_variability_plot_components(dff_for_variability, self.template)

    def get_cache_plot(self, filters=None):
        profiling_df = self.dm.get_profiling_data(filters)
        return create_cache_performance_comparison(profiling_df, self.template)

    def get_ipc_plot(self, filters=None):
        profiling_df = self.dm.get_profiling_data(filters)
        return create_ipc_comparison_plot(profiling_df, self.template)

    def get_stalls_plot(self, filters=None):
        profiling_df = self.dm.get_profiling_data(filters)
        return create_stalls_analysis_plot(profiling_df, self.template)

    def get_generic_profiling_plot(self, category, filters=None):
        profiling_df_categorized = self.dm.get_profiling_data(filters)
        category_df = profiling_df_categorized[profiling_df_categorized['category'] == category]
        if category_df.empty:
            fig = go.Figure().add_annotation(text=f"No data for profiling category: {category}", x=0.5, y=0.5, showarrow=False)
            return self.template.apply(fig, plot_title=f"{category} Analysis", height=500)

        if not category_df['event'].empty:
            event_counts = category_df['event'].value_counts()
            first_event = event_counts.index[0] if not event_counts.empty else category_df['event'].unique()[0]

            fig = go.Figure()
            for (example, version, p_size), group_data in category_df[category_df['event']==first_event].groupby(['example_name', 'version', 'problem_size']):
                group_data = group_data.sort_values('threads')
                if 'threads' in group_data and 'mean' in group_data and not group_data['mean'].isnull().all():
                    current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
                    current_dash = 'solid' if version == 'CARTS' else 'dash'
                    fig.add_trace(go.Scatter(
                        x=group_data['threads'], y=group_data['mean'], name=f"{example} (S:{p_size}) - {version}",
                        mode='lines+markers',
                        line=dict(color=current_color, width=2, dash=current_dash),
                        marker=dict(size=8, color=current_color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                        hovertemplate=f"<b>{example} - {version} (S:{p_size})</b><br>Event: {first_event}<br>Threads: %{{x}}<br>Mean: %{{y:,.2f}}<extra></extra>"
                    ))
            if not fig.data:
                fig.add_annotation(text=f"No plottable data for event: {first_event} in {category}", x=0.5, y=0.5, showarrow=False)
            return self.template.apply(fig, plot_title=f"{category} Analysis: {first_event}",
                                         xaxis_title="Threads", yaxis_title="Mean Value", height=550, legend_title="Ex (Size) - Ver")
        else:
            fig = go.Figure().add_annotation(text=f"No specific events found for category: {category}", x=0.5, y=0.5, showarrow=False)
            return self.template.apply(fig, plot_title=f"{category} Analysis", height=500)


# --- TableManager Class ---
class TableManager:
    def __init__(self, font_family):
        self.font_family = font_family

    def create_benchmark_summary_table(self, summary_df):
        if summary_df.empty:
            return html.Div("No benchmarking data to display after filtering.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})

        formatted_data = []
        for _, row in summary_df.iterrows():
            formatted_data.append({
                'Example': row['example_name'], 'Size': format_number(row['problem_size']),
                'Threads': format_number(row['threads']), 'Version': row['version'],
                'Mean (s)': format_number(row['mean_time']), 'Median (s)': format_number(row['median_time']),
                'Min (s)': format_number(row['min_time']), 'Max (s)': format_number(row['max_time']),
                'Stdev (s)': format_number(row['stdev_time']), 'Iterations': format_number(row['iterations']),
                'Correctness': str(row.get('correctness','UNKNOWN')).upper(),
                'Speedup': format_number(row.get('speedup_vs_other', None)),
                'Efficiency': format_number(row.get('parallel_efficiency', None))
            })

        columns = [{"name": col_name, "id": col_name} for col_name in formatted_data[0].keys()] if formatted_data else []

        return dash_table.DataTable(
            data=formatted_data, columns=columns, page_size=10,
            style_table={'overflowX': 'auto', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(44,111,187,0.10)'},
            style_cell={**StyleManager.TABLE_CELL_STYLE, 'fontFamily': self.font_family},
            style_header={**StyleManager.TABLE_HEADER_STYLE, 'fontFamily': self.font_family},
            style_cell_conditional=[
                {'if': {'column_id': c}, 'textAlign': 'right'} for c in ['Size', 'Threads', 'Mean (s)', 'Median (s)', 'Min (s)', 'Max (s)', 'Stdev (s)', 'Iterations', 'Speedup', 'Efficiency']
            ] + [
                {'if': {'column_id': c}, 'textAlign': 'center'} for c in ['Example', 'Version', 'Correctness']
            ],
            style_data_conditional=[
                {'if': {'column_id': 'Correctness', 'filter_query': '{Correctness} = CORRECT'}, 'backgroundColor': '#e6fae6', 'color': '#1a7f37'},
                {'if': {'column_id': 'Correctness', 'filter_query': '{Correctness} != CORRECT'}, 'backgroundColor': '#fff3e6', 'color': '#b26a00'},
                {'if': {'column_id': 'Speedup', 'filter_query': '{Speedup} > 1'}, 'backgroundColor': '#e6fae6', 'color': '#1a7f37'},
                {'if': {'column_id': 'Speedup', 'filter_query': '{Speedup} < 1'}, 'backgroundColor': '#fff3e6', 'color': '#b26a00'},
                {'if': {'column_id': 'Efficiency', 'filter_query': '{Efficiency} > 0.8'}, 'backgroundColor': '#e6fae6', 'color': '#1a7f37'},
                {'if': {'column_id': 'Efficiency', 'filter_query': '{Efficiency} < 0.5'}, 'backgroundColor': '#fff3e6', 'color': '#b26a00'},
            ],
            sort_action='native', filter_action='native', style_as_list_view=True,
        )

    def create_profiling_summary_table(self, profiling_stats_df):
        if profiling_stats_df.empty:
            return html.Div("No profiling statistics to display after filtering or processing.", style={'padding': '10px', 'color': WARNING_COLOR})

        formatted_data = []
        for _, row in profiling_stats_df.iterrows():
            formatted_data.append({
                'Example': row.get('example_name', 'N/A'),
                'Size': format_number(row.get('problem_size', None)),
                'Threads': format_number(row.get('threads', None)),
                'Version': row.get('version', 'N/A'),
                'Event': row.get('event', 'N/A'),
                'Mean': format_number(row.get('mean', None)),
                'Median': format_number(row.get('median', None)),
                'Min': format_number(row.get('min', None)),
                'Max': format_number(row.get('max', None)),
                'Stdev': format_number(row.get('stdev', None)),
            })

        columns = [{"name": col_name, "id": col_name} for col_name in formatted_data[0].keys()] if formatted_data else []

        return dash_table.DataTable(
            data=formatted_data, columns=columns, page_size=10,
            style_table={'overflowX': 'auto', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(44,111,187,0.10)'},
            style_cell={**StyleManager.TABLE_CELL_STYLE, 'fontFamily': self.font_family},
            style_header={**StyleManager.TABLE_HEADER_STYLE, 'fontFamily': self.font_family},
            style_cell_conditional=[{'if': {'column_id': c}, 'textAlign': 'right'} for c in ['Size', 'Threads', 'Mean', 'Median', 'Min', 'Max', 'Stdev']] +
                                    [{'if': {'column_id': c}, 'textAlign': 'center'} for c in ['Example', 'Version', 'Event']],
            style_data_conditional=[
                {'if': {'column_id': 'Stdev', 'filter_query': '{Stdev} >= 0.1'}, 'backgroundColor': '#fff3e6', 'color': '#b26a00'},
                {'if': {'column_id': 'Stdev', 'filter_query': '{Stdev} < 0.1'}, 'backgroundColor': '#e6fae6', 'color': '#1a7f37'},
            ], style_as_list_view=True, sort_action='native', filter_action='native',
        )

# --- Helper function for plot wrappers ---
def create_plot_with_help(graph_component, help_text_md, plot_id_base):
    help_icon_id = {'type': 'help-icon-btn', 'index': plot_id_base}
    modal_id = {'type': 'help-modal', 'index': plot_id_base}
    close_button_id = {'type': 'close-help-modal-btn', 'index': plot_id_base}

    return html.Div([
        html.Div([
            graph_component,
            html.Button(
                "ⓘ",
                id=help_icon_id,
                n_clicks=0,
                style={
                    'fontSize': '1.2em', 'cursor': 'pointer', 'marginLeft': '10px',
                    'color': PRIMARY_COLOR, 'position': 'absolute', 'top': '10px', 'right': '10px',
                    'zIndex': '10', 'background': 'none', 'border': 'none', 'padding': '0'
                }
            )
        ], style={'position': 'relative', 'paddingTop': '30px'}),
        dbc.Modal(
            id=modal_id,
            is_open=False,
            children=[
                html.Div([
                    dcc.Markdown(help_text_md, style={'marginBottom': '20px'}),
                    html.Button("Close", id=close_button_id, n_clicks=0,
                                style={'backgroundColor': PRIMARY_COLOR, 'color': 'white', 'border': 'none',
                                       'borderRadius': '5px', 'padding': '8px 15px', 'cursor': 'pointer'})
                ], style=StyleManager.HELP_MODAL_CONTENT_STYLE)
            ],
            style=StyleManager.HELP_MODAL_STYLE,
            centered=True
        )
    ])

# --- Dash App Layout Definition --- #
app = dash.Dash(__name__, suppress_callback_exceptions=True,
                external_stylesheets=['https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css'])

# Common style for graphs within tabs
graph_style = {'width': '100%', 'height': '550px'} # Define a common height

app.layout = html.Div([
    dcc.Store(id='modal-store', data={'open': False, 'card': None}),
    dcc.Store(id='filters-store', data={'examples': [], 'threads': []}), # Default to empty, meaning "all"
    dcc.Store(id='filter-modal-store'),
    dcc.Store(id='results-type-store', data='both'),
    dcc.Store(id='results-store'),
    html.Div(id='dummy-output-for-resize', children=None), # Dummy div for clientside callback output, using 'children'

    html.Div([ # Main card content
        html.Div([ # Header section
            html.H1([
                html.Span("CARTS", style={'color': PRIMARY_COLOR, 'fontWeight': '700'}),
                " Performance Dashboard"
            ], style=StyleManager.HEADER_STYLE),
            html.Div("Benchmark Comparison between CARTS and OpenMP Implementations", style=StyleManager.SUBTITLE_STYLE),
        ], style={'marginBottom': '12px', 'paddingBottom': '4px'}),

        # Results path input
        html.Div([
            html.Label([
                html.I(className="fas fa-file-alt", style={'marginRight': '8px'}), "Results file:",
                html.Span(" ⓘ", title="Path to the results JSON file (e.g., output/performance_results.json)", style={'cursor': 'help', 'color': ACCENT_COLOR, 'marginLeft': '4px'})
            ], style={**StyleManager.LABEL_STYLE, 'width': 'auto', 'textAlign': 'right', 'marginRight': '15px', 'marginBottom': '0', 'alignSelf': 'center', 'minWidth': '120px'}),
            dcc.Input(id='results-path', type='text', value=DEFAULT_RESULTS_PATH, style={
                'width': '60%', 'marginRight': '12px', 'marginBottom': '0', 'textAlign': 'left', 'height': '42px',
                'lineHeight': '42px', 'padding': '0 15px', 'borderRadius': '8px', 'border': '1px solid #e0e0e0',
                'boxShadow': '0 2px 8px rgba(0,0,0,0.05)', 'fontSize': '1.05rem', 'alignSelf': 'center'
            }),
        ], style={'marginBottom': '16px', 'marginTop': '12px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'center', 'gap': '0'}),
        html.Div(id='data-status-message', style={'color': ACCENT_COLOR, 'fontWeight': '600', 'marginBottom': '16px', 'textAlign': 'center'}),

        # Key Summary Statistics section removed

        # Filter button
        html.Div([
            html.Button([html.I(className="fas fa-filter", style={'marginRight': '8px'}), "Filter Results"],
                        id='open-filter-modal-btn', n_clicks=0, style={
                            'backgroundColor': PRIMARY_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '8px',
                            'padding': '12px 36px', 'fontWeight': '600', 'margin': '0 auto', 'display': 'block',
                            'boxShadow': '0 4px 12px rgba(44,111,187,0.15)', 'cursor': 'pointer', 'fontSize': '1.05rem'}),
        ], style={'display': 'flex', 'justifyContent': 'center', 'alignItems': 'center', 'marginBottom': '28px', 'marginTop': '12px'}),

        # Average per example plot (outside tabs) - REMOVED
        # html.Div(id='average-per-example-bar-section'),

        # Benchmark Results Section (Tabs and Table)
        html.Div([
            html.H2("Benchmark Results", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '8px', 'marginBottom': '12px'}),
            dcc.Tabs(id='main-plot-tabs', value='scaling', children=[
                dcc.Tab(label="Scaling", value='scaling', children=[html.Div(id='scaling-plot-wrapper')]),
                dcc.Tab(label="Parallel Efficiency", value='efficiency', children=[html.Div(id='efficiency-plot-wrapper')]),
                dcc.Tab(label="Speedup (OMP/CARTS)", value='speedup', children=[html.Div(id='speedup-plot-wrapper')]),
                dcc.Tab(label="Slowdown (CARTS/OMP)", value='slowdown', children=[html.Div(id='slowdown-plot-wrapper')]),
                dcc.Tab(label="Variability", value='variability', children=[
                    html.Div(id='variability-content-area', children=[ # This Div will hold the sub-tabs
                        dcc.Tabs(id='variability-sub-tabs', value=None, children=[], style={'fontSize': '0.9em', 'marginTop': '10px'})
                    ], style={'display': 'none'}) # Initially hidden
                ]),
            ], style={'backgroundColor': '#f7fbff', 'borderRadius': '8px', 'padding': '8px 8px',
                       'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'marginBottom': '18px', 'fontSize': '1.01em'}),
            html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),
            html.H2("Summary Table", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '24px', 'marginBottom': '12px'}),
            html.Button("Download Benchmark Summary CSV", id="download-benchmark-btn", n_clicks=0, style={
                'backgroundColor': ACCENT_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '7px',
                'padding': '8px 18px', 'fontWeight': 'bold', 'fontSize': '1.01rem', 'marginBottom': '10px'}),
            dcc.Download(id="download-benchmark-csv"),
            html.Div(id='results-table-section'),
        ], style={'backgroundColor': '#fff', 'borderRadius': '14px', 'boxShadow': '0 4px 16px rgba(44,111,187,0.08)',
                  'padding': '18px 18px 10px 18px', 'marginBottom': '24px', 'border': '1.5px solid #e0e0e0'}),

        html.Div(id='filter-modal'),
        html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),

        # Profiling Analysis Section
        html.Div([
            html.H2("Profiling Analysis", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '8px', 'marginBottom': '12px'}),
            dcc.Dropdown(id='profiling-group-dropdown', options=[], value='Cache', clearable=False,
                         style={'width': '260px', 'display': 'inline-block', 'marginBottom': '10px', 'fontSize': '1.08em'}),
            html.Div(id='perf-event-analysis-section'), # Wrapper for profiling plot
            html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '25px 0'}),
            html.Button("Download Profiling Summary CSV", id="download-profiling-btn", n_clicks=0, style={
                'backgroundColor': ACCENT_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '7px',
                'padding': '8px 18px', 'fontWeight': 'bold', 'fontSize': '1.01rem', 'marginBottom': '10px'}),
            dcc.Download(id="download-profiling-csv"),
            html.Div(id='profiling-summary-section'),
        ], style={'backgroundColor': '#fff', 'borderRadius': '14px', 'boxShadow': '0 4px 16px rgba(44,111,187,0.08)',
                  'padding': '18px 18px 10px 18px', 'marginBottom': '24px', 'border': '1.5px solid #e0e0e0'}),

        html.Hr(style={'border': 'none', 'borderTop': '2px solid #d0e3f7', 'margin': '18px 0'}),
        html.Div(id='system-info-card'),

        html.Div(id='summary-modal', style={'display':'none'}),
        dcc.Dropdown(id='perf-event-dropdown', style={'display': 'none'}),

    ], style=StyleManager.CARD_STYLE),

    # Footer
    html.Div([
        html.Span("CARTS Benchmarking Report | "),
        html.A("GitHub", href="https://github.com/ARTS-Lab/carts", target="_blank", style={'color': PRIMARY_COLOR, 'textDecoration': 'none', 'fontWeight': 'bold'}),
        html.Span(" | For scientific reproducibility. "),
        html.Span(f"© {datetime.datetime.now().year}")
    ], style={'textAlign': 'center', 'color': NEUTRAL_COLOR, 'marginTop': '40px', 'fontSize': '1.05rem',
              'paddingBottom': '20px', 'borderTop': '1px solid #e0e0e0', 'paddingTop': '20px',
              'maxWidth': '1400px', 'margin': '40px auto 0 auto'}),
], style=StyleManager.MAIN_DIV_STYLE)

# --- Callbacks --- #

@app.callback(
    Output('results-store', 'data'),
    [Input('results-path', 'value')]
)
def load_results_to_store(path):
    if not path: return None
    df, error = load_results(path)
    if error or df is None or df.empty: return None
    return df.to_dict('records')

@app.callback(
    Output('data-status-message', 'children'),
    [Input('results-store', 'data'), Input('results-path', 'value')],
    prevent_initial_call=True
)
def update_data_status(data_records, path):
    if not path: return "Please provide a results file path."
    if data_records is None:
        _, error_msg = load_results(path)
        return error_msg if error_msg else "Failed to load data or data is empty."
    if not data_records: return "No results found in the loaded file."
    return f"Data loaded successfully from {path}"

# Key Summary Stats callback removed

# Clientside callback to resize plots in tabs
app.clientside_callback(
    """
    function(main_tab_value, variability_sub_tab_value, profiling_category_value) {
        setTimeout(function() {
            var graph_ids_to_resize = [];

            // Static graphs outside variability tab
            var static_graph_ids = [
                'scaling-graph', 'efficiency-graph', 'speedup-graph', 
                'slowdown-graph', 
                // 'average-per-example-graph', // Removed
                'perf-event-graph'
            ];
            static_graph_ids.forEach(function(graph_id) {
                var graphElement = document.getElementById(graph_id); 
                if (graphElement && graphElement.offsetParent !== null) {
                    var plotlyDiv = graphElement.querySelector('.js-plotly-plot');
                    if (plotlyDiv) { // Resize the inner plotly div
                        Plotly.Plots.resize(plotlyDiv);
                    } else { // Fallback if structure is different
                         try { Plotly.Plots.resize(graphElement); } catch(e) { /* console.error('Error resizing static plot (fallback):', graph_id, e); */ }
                    }
                }
            });

            // If variability tab is active, find the graph in the active sub-tab
            if (main_tab_value === 'variability' && variability_sub_tab_value) {
                var sanitized_sub_tab_value = variability_sub_tab_value.replace(/\\W+/g, '_');
                var active_variability_graph_id = 'variability-graph-' + sanitized_sub_tab_value;
                 var graphElement = document.getElementById(active_variability_graph_id);
                 if (graphElement && graphElement.offsetParent !== null) {
                    var plotlyDiv = graphElement.querySelector('.js-plotly-plot');
                    if (plotlyDiv) {
                        Plotly.Plots.resize(plotlyDiv);
                    } else {
                        try { Plotly.Plots.resize(graphElement); } catch(e) { /* console.error('Error resizing dynamic plot (fallback):', active_variability_graph_id, e); */ }
                    }
                 }
            }
            
        }, 250); 
        return window.dash_clientside.no_update;
    }
    """,
    Output('dummy-output-for-resize', 'children'),
    [Input('main-plot-tabs', 'value'),
     Input('variability-sub-tabs', 'value'), 
     Input('profiling-group-dropdown', 'value')]
)

@app.callback(
    [Output('scaling-plot-wrapper', 'children'),
     Output('efficiency-plot-wrapper', 'children'),
     Output('speedup-plot-wrapper', 'children'),
     Output('slowdown-plot-wrapper', 'children')],
    [Input('filters-store', 'data'),
     Input('results-store', 'data')]
)
def update_benchmark_graphs(filters, data_records):
    if not data_records:
        empty_fig = default_plot_template.apply(go.Figure().add_annotation(text="No data to display.", xref="paper", yref="paper", showarrow=False))
        empty_plot_component = create_plot_with_help(dcc.Graph(figure=empty_fig, style=graph_style, id="empty-graph"), "No data available for this plot.", "empty")
        return [empty_plot_component] * 4

    data_manager = DataManager(data_records)
    plot_manager = PlotManager(default_plot_template, data_manager)

    scaling_fig = plot_manager.get_scaling_plot(filters)
    scaling_help = """
**Execution Time Scaling**

This plot shows the mean execution time (log scale) against the number of threads for different examples and problem sizes.
- **CARTS** version is shown with a solid blue line.
- **OMP** version is shown with a dashed orange line.
- Each line represents a unique combination of example, version, and problem size.
    """
    scaling_plot_component = create_plot_with_help(dcc.Graph(id='scaling-graph', figure=scaling_fig, style=graph_style), scaling_help, "scaling")

    efficiency_fig = plot_manager.get_efficiency_plot(filters)
    efficiency_help = """
**Parallel Efficiency (Strong Scaling)**

This plot illustrates how well the parallel versions (CARTS and OMP) utilize additional threads compared to their single-threaded performance.
- Efficiency is calculated as: `Time(1 thread) / (Time(N threads) * N threads)`.
- An ideal efficiency is 1.0 (dashed grey line).
- Values closer to 1.0 indicate better scalability.
    """
    efficiency_plot_component = create_plot_with_help(dcc.Graph(id='efficiency-graph', figure=efficiency_fig, style=graph_style), efficiency_help, "efficiency")

    speedup_fig = plot_manager.get_speedup_plot(filters)
    speedup_help = """
**OMP vs CARTS Performance (OMP Time / CARTS Time)**

This plot shows the speedup of CARTS relative to OMP.
- Calculated as: `Mean OMP Time / Mean CARTS Time` for the same example, problem size, and thread count.
- Values > 1 (green shaded area) indicate CARTS is faster.
- Values < 1 (red shaded area) indicate OMP is faster.
- A value of 1 (dashed grey line) means equal performance.
    """
    speedup_plot_component = create_plot_with_help(dcc.Graph(id='speedup-graph', figure=speedup_fig, style=graph_style), speedup_help, "speedup")
    
    slowdown_fig = plot_manager.get_slowdown_plot(filters)
    slowdown_help = """
**CARTS vs OMP Performance (CARTS Time / OMP Time)**

This plot shows the slowdown of CARTS relative to OMP (inverse of the speedup plot).
- Calculated as: `Mean CARTS Time / Mean OMP Time` for the same example, problem size, and thread count.
- Values > 1 (red shaded area) indicate CARTS is slower.
- Values < 1 (green shaded area) indicate CARTS is faster.
- A value of 1 (dashed grey line) means equal performance.
    """
    slowdown_plot_component = create_plot_with_help(dcc.Graph(id='slowdown-graph', figure=slowdown_fig, style=graph_style), slowdown_help, "slowdown")
    
    return scaling_plot_component, efficiency_plot_component, speedup_plot_component, slowdown_plot_component

@app.callback(
    [Output('variability-sub-tabs', 'children'),
     Output('variability-sub-tabs', 'value'),
     Output('variability-content-area', 'style')], 
    [Input('filters-store', 'data'), 
     Input('results-store', 'data'),
     Input('main-plot-tabs', 'value')] 
)
def update_variability_plots_section(filters, data_records, active_main_tab):
    if active_main_tab != 'variability' or not data_records:
        return [], None, {'display': 'none'} 

    data_manager = DataManager(data_records)
    df_full_filtered = data_manager.get_filtered_data(filters)

    if df_full_filtered.empty or 'example_name' not in df_full_filtered.columns:
        return [html.Div("No examples found for variability plots after filtering.", style={'padding':'10px', 'textAlign':'center', 'color': WARNING_COLOR})], None, {'display': 'block'}

    df_bench = df_full_filtered[df_full_filtered.get('run_type', pd.Series(dtype=str)).astype(str).str.lower() == 'no_profile'].copy()
    if df_bench.empty:
        return [html.Div("No benchmark runs data for variability plot.", style={'padding': '10px', 'textAlign': 'center', 'color': WARNING_COLOR})], None, {'display': 'block'}

    records = []
    for _, row_series in df_bench.iterrows():
        row = row_series.to_dict()
        times_raw = row.get('times_seconds')
        current_times = []
        if isinstance(times_raw, str):
            try: current_times = [float(t.strip()) for t in times_raw.split(',') if t.strip()]
            except ValueError: pass
        elif isinstance(times_raw, list):
            current_times = [float(t) for t in times_raw if isinstance(t, (int, float))]
        elif isinstance(times_raw, (int, float)):
            current_times = [float(times_raw)]
        
        for t_val in current_times:
            records.append({
                'example_name': row.get('example_name', 'Unknown Example'),
                'problem_size': str(row.get('problem_size', 'N/A')),
                'threads': row.get('threads', 1),
                'version': row.get('version', 'Unknown Version'),
                'time': t_val
            })
    
    flat_df_for_variability = pd.DataFrame(records)
    if flat_df_for_variability.empty:
        return [html.Div("No timing data points to plot variability.", style={'padding': '10px', 'textAlign': 'center', 'color': WARNING_COLOR})], None, {'display': 'block'}

    flat_df_for_variability['threads_numeric'] = pd.to_numeric(flat_df_for_variability['threads'], errors='coerce')
    flat_df_for_variability.dropna(subset=['threads_numeric'], inplace=True)
    flat_df_for_variability['threads'] = flat_df_for_variability['threads_numeric'].astype(int)

    unique_examples = sorted(flat_df_for_variability['example_name'].unique())
    if not unique_examples:
        return [html.Div("No examples available for variability plots.", style={'padding':'10px', 'textAlign':'center', 'color': WARNING_COLOR})], None, {'display': 'block'}

    sub_tabs_children = []
    variability_help = """
**Execution Time Variability**

These box plots show the distribution of execution times (log scale) for each combination of example, problem size, version, and thread count.
- Each box represents the interquartile range (IQR), with the median as a line.
- Whiskers extend to 1.5x IQR. Points beyond are outliers.
- Useful for observing consistency of runtimes.
    """
    for example_name in unique_examples:
        example_specific_df = flat_df_for_variability[flat_df_for_variability['example_name'] == example_name]
        if example_specific_df.empty or 'time' not in example_specific_df.columns: 
            fig_content = html.Div(f"No plottable variability data for example: {example_name}", style={'padding':'10px', 'textAlign':'center'})
        else:
            fig = create_single_example_variability_plot(example_specific_df, example_name, default_plot_template)
            sanitized_example_name = re.sub(r'\W+', '_', example_name)
            graph_component = dcc.Graph(
                id=f'variability-graph-{sanitized_example_name}',
                figure=fig,
                className='resizable-variability-plot', # For JS resize
                style=graph_style
            )
            fig_content = create_plot_with_help(graph_component, variability_help, f"variability-{sanitized_example_name}")
        
        sub_tabs_children.append(dcc.Tab(label=example_name, value=sanitized_example_name, children=[fig_content]))

    if not sub_tabs_children:
        return [html.Div("No variability data to display after filtering.", style={'padding':'10px', 'textAlign':'center', 'color': WARNING_COLOR})], None, {'display': 'block'}

    default_active_sub_tab = re.sub(r'\W+', '_', unique_examples[0]) if unique_examples else None
    
    return sub_tabs_children, default_active_sub_tab, {'display': 'block'}


@app.callback(
    Output('results-table-section', 'children'),
    [Input('filters-store', 'data'),
     Input('results-store', 'data')]
)
def update_benchmark_table(filters, data_records):
    if not data_records:
        return html.Div("No data loaded to display table.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})

    data_manager = DataManager(data_records)
    table_manager = TableManager(font_family=default_plot_template.font_family)
    summary_df_for_table = data_manager.get_benchmark_summary(filters)
    return table_manager.create_benchmark_summary_table(summary_df_for_table)


# Average per example section and callback removed

@app.callback(
    Output('perf-event-analysis-section', 'children'),
    [Input('profiling-group-dropdown', 'value'),
     Input('filters-store', 'data'),
     Input('results-store', 'data')]
)
def update_perf_event_analysis_figure(selected_category, filters, data_records):
    if not data_records:
        empty_fig = default_plot_template.apply(go.Figure().add_annotation(text="Load data to see profiling analysis.", xref="paper", yref="paper", showarrow=False))
        return create_plot_with_help(dcc.Graph(id='perf-event-graph', figure=empty_fig, style=graph_style), "No profiling data loaded.", "perf-event-empty")

    if not selected_category:
        empty_fig = default_plot_template.apply(go.Figure().add_annotation(text="Select a profiling category.", xref="paper", yref="paper", showarrow=False))
        return create_plot_with_help(dcc.Graph(id='perf-event-graph', figure=empty_fig, style=graph_style), "Please select a profiling category from the dropdown.", "perf-event-select")


    data_manager = DataManager(data_records)
    plot_manager = PlotManager(default_plot_template, data_manager)
    
    help_text = f"**{selected_category} Analysis**\n\nThis plot shows performance counter data related to {selected_category.lower()}.\n"
    fig = go.Figure() # Default empty figure

    if selected_category == 'Cache':
        fig = plot_manager.get_cache_plot(filters)
        # Determine which events were actually plotted for more specific help text
        profiling_df = data_manager.get_profiling_data(filters)
        if not profiling_df[profiling_df['event'] == 'cache-miss-rate'].empty:
            help_text += "- Shows L1/L2 cache miss rate (cache-misses / cache-references).\n- Calculated as the mean of `cache-misses` divided by the mean of `cache-references` for each group.\n- Events used: `cache-references`, `cache-misses`."
        else:
            cache_events_fallback = ['cache-misses', 'L1-dcache-load-misses', 'LLC-load-misses']
            plotted_event = [e for e in cache_events_fallback if not profiling_df[profiling_df['event'] == e].empty]
            if plotted_event:
                help_text += f"- Shows mean count of '{plotted_event[0]}'.\n- Event used: `{plotted_event[0]}` (as 'cache-miss-rate' was not available or had no data)."
            else:
                help_text += "- No specific cache event data found to plot."

    elif selected_category == 'CPU':
        fig = plot_manager.get_ipc_plot(filters)
        help_text += "- Shows Instructions Per Cycle (IPC).\n- Calculated as: mean `instructions` / mean `cycles` for each group.\n- Events used: `instructions`, `cycles`."
    elif selected_category == 'Stalls':
        fig = plot_manager.get_stalls_plot(filters)
        # Determine which stall event was plotted
        profiling_df = data_manager.get_profiling_data(filters)
        stall_events_present = [e for e in profiling_df['event'].unique() if 'stall' in e.lower() or 'resource_stalls' in e.lower()]
        event_to_plot = None; priority_stalls = ['cycle_activity.stalls_total', 'stalls_total', 'resource_stalls.any']
        for p_event in priority_stalls:
            if p_event in stall_events_present: event_to_plot = p_event; break
        if not event_to_plot and stall_events_present: event_to_plot = stall_events_present[0]
        
        if event_to_plot:
            help_text += f"- Shows data for event: `{event_to_plot}`.\n"
            if 'cycle_activity' in event_to_plot and not profiling_df[profiling_df['event'] == 'cycles'].empty:
                 help_text += "- Displayed as a percentage of total cycles (mean `{event_to_plot}` / mean `cycles`). Events used: `{event_to_plot}`, `cycles`."
            else:
                 help_text += f"- Displayed as raw mean count (log scale). Event used: `{event_to_plot}`."
        else:
            help_text += "- No specific stall event data found to plot."

    else: # Generic plot
        fig = plot_manager.get_generic_profiling_plot(selected_category, filters)
        # Try to get the specific event plotted by the generic function
        profiling_df_cat = data_manager.get_profiling_data(filters)
        category_df_generic = profiling_df_cat[profiling_df_cat['category'] == selected_category]
        if not category_df_generic.empty and not category_df_generic['event'].empty:
            event_counts_generic = category_df_generic['event'].value_counts()
            first_event_generic = event_counts_generic.index[0] if not event_counts_generic.empty else category_df_generic['event'].unique()[0]
            help_text += f"- Shows mean value for event: `{first_event_generic}`."
        else:
            help_text += "- No specific event data found for this category."

    return create_plot_with_help(dcc.Graph(id='perf-event-graph', figure=fig, style=graph_style), help_text, "perf-event")

@app.callback(
    Output('profiling-summary-section', 'children'),
    [Input('filters-store', 'data'), Input('results-store', 'data')]
)
def update_profiling_summary_table(filters, data_records):
    if not data_records:
        return html.Div("Load data to see profiling summary table.", style={'padding':'10px', 'textAlign':'center'})

    data_manager = DataManager(data_records)
    table_manager = TableManager(font_family=default_plot_template.font_family)
    profiling_df = data_manager.get_profiling_data(filters)
    return table_manager.create_profiling_summary_table(profiling_df)


@app.callback(
    Output('profiling-group-dropdown', 'options'),
    [Input('results-store', 'data')]
)
def update_profiling_dropdown_options(data_records):
    if not data_records:
        return []
    data_manager = DataManager(data_records)
    profiling_df = data_manager.get_profiling_data() # Get all profiling data to find categories
    if profiling_df.empty or 'category' not in profiling_df.columns:
        return []
    categories = sorted(profiling_df['category'].unique())
    return [{'label': cat, 'value': cat} for cat in categories]


# --- Filter Modal Callbacks & Rendering ---
def render_filter_modal(modal_state, example_opts, thread_opts, size_opts=None, sysinfo=None):
    ex_val = modal_state.get('examples', [opt['value'] for opt in example_opts] if example_opts else [])
    th_val = modal_state.get('threads', [opt['value'] for opt in thread_opts] if thread_opts else [])
    sz_val = modal_state.get('sizes', [opt['value'] for opt in size_opts] if size_opts else [])

    select_all_style = {'color': ACCENT_COLOR, 'fontWeight': 'bold', 'cursor': 'pointer', 'fontSize': '0.98em', 'marginLeft': '8px', 'textDecoration': 'underline'}
    divider_style = {'height': '1px', 'background': '#e6f0fa', 'margin': '20px 0 14px 0', 'border': 'none'}
    checklist_card_style = {
        'background': '#f7fbff', 'borderRadius': '10px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)',
        'padding': '10px 10px', 'marginBottom': '16px', 'width': '95%', 'maxHeight': '120px', 'overflowY': 'auto',
        'border': '1.2px solid #d0e3f7', 'fontSize': '1.00rem',
        'fontFamily': default_plot_template.font_family, 'wordBreak': 'break-word', 'overflowX': 'hidden',
    }
    accent_bar_style = {
        'height': '7px', 'width': '100%', 'background': f'linear-gradient(90deg, {PRIMARY_COLOR} 0%, {ACCENT_COLOR} 100%)',
        'borderTopLeftRadius': '14px', 'borderTopRightRadius': '14px', 'marginBottom': '10px',
    }
    section_header_style = {'fontWeight': 'bold', 'fontSize': '1.08rem', 'color': PRIMARY_COLOR, 'marginTop': '12px', 'marginBottom': '6px', 'letterSpacing': '0.2px'}

    if not example_opts and not thread_opts and (not size_opts or not any(size_opts)):
        return html.Div([
            html.Div([
                html.Div(style=accent_bar_style),
                html.Button('×', id={'type': 'close-filter-modal-btn', 'index': 0}, n_clicks=0, style={**StyleManager.FILTER_MODAL_CLOSE_STYLE, 'fontSize': '2.2em'}),
                html.H3('Filter Results', style={'color': PRIMARY_COLOR, 'marginBottom': '14px', 'textAlign': 'center', 'fontSize': '1.3rem', 'fontWeight': 'bold'}),
                html.Div('No data loaded or no filterable options available.', style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'textAlign': 'center', 'margin': '24px 0', 'fontSize': '1.01rem'})
            ], style={**StyleManager.FILTER_MODAL_CONTENT_STYLE, 'display': 'flex', 'flexDirection': 'column', 'alignItems': 'center', 'justifyContent': 'center', 'minHeight': '220px', 'maxHeight': '70vh', 'overflowY': 'auto', 'boxShadow': '0 8px 32px rgba(31,119,180,0.18)'}),
        ], style={**StyleManager.FILTER_MODAL_OVERLAY_STYLE, 'backdropFilter': 'blur(2px)'})

    return html.Div([
        html.Div([
            html.Div(style=accent_bar_style),
            html.Div([
                html.H3('Filter Results', style={'color': PRIMARY_COLOR, 'marginBottom': '14px', 'textAlign': 'center', 'fontSize': '1.3rem', 'fontWeight': 'bold', 'flex': 1, 'letterSpacing': '0.2px'}),
                html.Button('×', id={'type': 'close-filter-modal-btn', 'index': 0}, n_clicks=0, style={**StyleManager.FILTER_MODAL_CLOSE_STYLE, 'fontSize': '2.2em'}),
            ], style={'display': 'flex', 'alignItems': 'center', 'justifyContent': 'space-between', 'width': '100%', 'marginBottom': '8px', 'paddingRight': '10px'}),

            html.Div([
                html.Label('Examples:', style=section_header_style),
                html.Span('Select All', id='select-all-examples', n_clicks=0, style=select_all_style),
                html.Span(' / ', style={'color': '#aaa', 'margin': '0 2px'}),
                html.Span('Clear All', id='clear-all-examples', n_clicks=0, style=select_all_style),
                dcc.Checklist(id='filter-example-checklist', options=example_opts or [], value=ex_val, inline=False, style=checklist_card_style, inputStyle={"marginRight":"5px"}),
            ], style={'width': '100%', 'paddingLeft': '18px', 'paddingRight': '18px'}),
            html.Hr(style=divider_style),

            html.Div([
                html.Label('Threads:', style=section_header_style),
                html.Span('Select All', id='select-all-threads', n_clicks=0, style=select_all_style),
                html.Span(' / ', style={'color': '#aaa', 'margin': '0 2px'}),
                html.Span('Clear All', id='clear-all-threads', n_clicks=0, style=select_all_style),
                dcc.Checklist(id='filter-thread-checklist', options=thread_opts or [], value=th_val, inline=False, style=checklist_card_style, inputStyle={"marginRight":"5px"}),
            ], style={'width': '100%', 'paddingLeft': '18px', 'paddingRight': '18px'}),
            html.Hr(style=divider_style),

            html.Div([
                html.Label('Size:', style=section_header_style),
                html.Span('Select All', id='select-all-sizes', n_clicks=0, style=select_all_style),
                html.Span(' / ', style={'color': '#aaa', 'margin': '0 2px'}),
                html.Span('Clear All', id='clear-all-sizes', n_clicks=0, style=select_all_style),
                dcc.Checklist(id='filter-size-checklist', options=size_opts or [], value=sz_val, inline=False, style=checklist_card_style, inputStyle={"marginRight":"5px"}),
            ], style={'width': '100%', 'paddingLeft': '18px', 'paddingRight': '18px'}) if size_opts and any(size_opts) else html.Div(),

            html.Hr(style=divider_style) if size_opts and any(size_opts) else html.Div(),

            html.Div([
                html.Button('Clear All Filters', id={'type': 'reset-filters-btn', 'index': 0}, n_clicks=0, style={'backgroundColor': PRIMARY_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '7px', 'padding': '10px 22px', 'marginRight': '18px', 'fontWeight': 'bold', 'fontSize': '1.01rem', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)', 'transition': 'background 0.2s, box-shadow 0.2s', 'marginTop': '8px', 'cursor':'pointer'}),
                html.Button('Apply Filters', id={'type': 'apply-filters-btn', 'index': 0}, n_clicks=0, style={'backgroundColor': ACCENT_COLOR, 'color': 'white', 'border': 'none', 'borderRadius': '7px', 'padding': '10px 22px', 'fontWeight': 'bold', 'fontSize': '1.01rem', 'boxShadow': '0 2px 8px rgba(255,127,14,0.13)', 'transition': 'background 0.2s, box-shadow 0.2s', 'marginTop': '8px', 'cursor':'pointer'}),
            ], style={'marginTop': '16px', 'marginBottom': '4px', 'display': 'flex', 'justifyContent': 'center', 'gap': '12px'}),
        ], style={**StyleManager.FILTER_MODAL_CONTENT_STYLE, 'display': 'flex', 'flexDirection': 'column', 'alignItems': 'center', 'maxHeight': '80vh', 'overflowY': 'auto'}),
    ], style={**StyleManager.FILTER_MODAL_OVERLAY_STYLE, 'backdropFilter': 'blur(2px)'})

@app.callback( Output('filter-example-checklist', 'value'),
    [Input('select-all-examples', 'n_clicks'), Input('clear-all-examples', 'n_clicks')],
    [State('filter-example-checklist', 'options')], prevent_initial_call=True )
def toggle_examples_select_all(select_all, clear_all, options):
    ctx = callback_context
    if not ctx.triggered or not options: raise PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    if btn_id == 'select-all-examples': return [opt['value'] for opt in options]
    elif btn_id == 'clear-all-examples': return []
    raise PreventUpdate

@app.callback( Output('filter-thread-checklist', 'value'),
    [Input('select-all-threads', 'n_clicks'), Input('clear-all-threads', 'n_clicks')],
    [State('filter-thread-checklist', 'options')], prevent_initial_call=True )
def toggle_threads_select_all(select_all, clear_all, options):
    ctx = callback_context
    if not ctx.triggered or not options: raise PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    if btn_id == 'select-all-threads': return [opt['value'] for opt in options]
    elif btn_id == 'clear-all-threads': return []
    raise PreventUpdate

@app.callback( Output('filter-size-checklist', 'value'),
    [Input('select-all-sizes', 'n_clicks'), Input('clear-all-sizes', 'n_clicks')],
    [State('filter-size-checklist', 'options')], prevent_initial_call=True)
def toggle_sizes_select_all(select_all, clear_all, options):
    ctx = callback_context
    if not ctx.triggered or not options: raise PreventUpdate
    btn_id = ctx.triggered[0]['prop_id'].split('.')[0]
    if btn_id == 'select-all-sizes': return [opt['value'] for opt in options]
    elif btn_id == 'clear-all-sizes': return []
    raise PreventUpdate

@app.callback(
    [Output('filter-modal', 'children'), Output('filter-modal-store', 'data', allow_duplicate=True)],
    [Input('open-filter-modal-btn', 'n_clicks'),
     Input({'type': 'close-filter-modal-btn', 'index': ALL}, 'n_clicks'),
     Input({'type': 'reset-filters-btn', 'index': ALL}, 'n_clicks')],
    [State('filters-store', 'data'), State('results-store', 'data'), State('filter-modal-store', 'data')],
    prevent_initial_call=True
)
def filter_modal_open_close_reset(open_n, close_n_list, reset_n_list, active_filters, results_data_records, modal_selection_state):
    ctx = callback_context
    if not ctx.triggered: raise PreventUpdate

    triggered_id_obj = ctx.triggered[0]
    triggered_id = triggered_id_obj['prop_id'].split('.')[0]
    if triggered_id.startswith('{'):
        try:
            triggered_id_dict = json.loads(triggered_id.replace("'", "\""))
            triggered_id_type = triggered_id_dict.get('type')
        except: triggered_id_type = None
    else: triggered_id_type = triggered_id

    data_manager = DataManager(results_data_records)
    example_opts, thread_opts, size_opts, sysinfo = [], [], [], None
    if not data_manager.raw_df.empty:
        if 'example_name' in data_manager.raw_df.columns: example_opts = [{'label': str(ex), 'value': ex} for ex in sorted(data_manager.raw_df['example_name'].unique())]
        if 'threads' in data_manager.raw_df.columns: thread_opts = [{'label': str(t), 'value': t} for t in sorted(data_manager.raw_df['threads'].dropna().unique().astype(int))]
        if 'problem_size' in data_manager.raw_df.columns: size_opts = [{'label': str(s), 'value': s} for s in sorted(data_manager.raw_df['problem_size'].dropna().unique())]
        sysinfo = data_manager.get_system_info()

    default_modal_selections = {
        'examples': [opt['value'] for opt in example_opts],
        'threads': [opt['value'] for opt in thread_opts],
        'sizes': [opt['value'] for opt in size_opts],
        'correctness': 'ALL'
    }

    if triggered_id_type == 'open-filter-modal-btn':
        current_selections = modal_selection_state if modal_selection_state and isinstance(modal_selection_state, dict) else active_filters if active_filters and any(active_filters.values()) else default_modal_selections
        for key, default_val in default_modal_selections.items():
            if key not in current_selections or not current_selections[key]: # Ensure all keys are present
                current_selections[key] = default_val
        return render_filter_modal(current_selections, example_opts, thread_opts, size_opts, sysinfo), current_selections

    elif triggered_id_type == 'close-filter-modal-btn' and any(c for c in close_n_list if c is not None):
        return None, modal_selection_state

    elif triggered_id_type == 'reset-filters-btn' and any(r for r in reset_n_list if r is not None):
        return render_filter_modal(default_modal_selections, example_opts, thread_opts, size_opts, sysinfo), default_modal_selections

    return dash.no_update, modal_selection_state


@app.callback(
    [Output('filters-store', 'data'), Output('filter-modal-store', 'data', allow_duplicate=True), Output('filter-modal', 'children', allow_duplicate=True)],
    [Input({'type': 'apply-filters-btn', 'index': ALL}, 'n_clicks')],
    [State('filter-example-checklist', 'value'), State('filter-thread-checklist', 'value'),
     State('filter-size-checklist', 'value'), State('filter-modal-store', 'data')],
    prevent_initial_call=True
)
def apply_filters_callback(apply_n_list, example_val, thread_val, size_val, modal_selection_state):
    ctx = callback_context
    if not ctx.triggered or not any(n for n in apply_n_list if n is not None):
        raise PreventUpdate
    new_filters = {
        'examples': example_val if example_val is not None else [],
        'threads': thread_val if thread_val is not None else [],
        'sizes': size_val if size_val is not None else [],
        'correctness': modal_selection_state.get('correctness', 'ALL') if modal_selection_state else 'ALL'
    }
    updated_modal_selection_state = new_filters.copy()
    return new_filters, updated_modal_selection_state, None


@app.callback(
    Output('system-info-card', 'children'),
    [Input('results-store', 'data')]
)
def update_system_info_card(results_data_records):
    if not results_data_records: return None
    data_manager = DataManager(results_data_records)
    sysinfo = data_manager.get_system_info()
    return render_system_info(sysinfo)

def render_system_info(sysinfo):
    if not sysinfo or not isinstance(sysinfo, dict):
        return html.Div("System information not available.", style={'padding': '10px', 'color': NEUTRAL_COLOR})
    items = []
    sys_map = {'os_platform': 'System', 'cpu_model': 'CPU', 'cpu_cores': 'Cores',
               'memory_total_mb': 'RAM', 'clang_version': 'Clang', 'timestamp': 'Timestamp'}
    for key, label in sys_map.items():
        if sysinfo.get(key):
            value = sysinfo[key]
            if key == 'memory_total_mb' and isinstance(value, (int, float)): value = f"{value:,.0f} MB"
            items.append(html.Div([html.B(f"{label}: "), html.Span(str(value))], style={'marginBottom': '5px'}))
    if not items: return html.Div("No detailed system information found.", style={'padding': '10px', 'color': NEUTRAL_COLOR})
    return html.Div([
        html.H3("System Information", style={'color': PRIMARY_COLOR, 'fontWeight': 'bold', 'marginTop': '24px', 'marginBottom': '12px'}),
        html.Div(items, style={'marginTop': '10px', 'fontSize': '0.98rem', 'color': NEUTRAL_COLOR, 'backgroundColor': '#f7fbff',
                                'borderRadius': '8px', 'padding': '12px 18px', 'boxShadow': '0 2px 8px rgba(31,119,180,0.07)'})
    ], style={'marginBottom': '18px'})


# average_per_example_figure function removed

def create_scaling_plot(summary_df, template):
    if summary_df.empty or 'mean_time' not in summary_df.columns:
        fig = go.Figure().add_annotation(text="No scaling data available", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Execution Time Scaling", height=400, xaxis_title="", yaxis_title="")

    examples = sorted(summary_df['example_name'].unique())
    num_examples = len(examples)
    if num_examples == 0:
        fig = go.Figure().add_annotation(text="No examples for scaling plot", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Execution Time Scaling", height=400, xaxis_title="", yaxis_title="")

    cols = min(3, num_examples); rows = (num_examples + cols - 1) // cols
    fig = make_subplots(rows=rows, cols=cols, subplot_titles=[f"Ex: {ex}" for ex in examples],
                        shared_xaxes=True, shared_yaxes=False, vertical_spacing=0.22, horizontal_spacing=0.08)
    legend_added = set()
    for i, example in enumerate(examples):
        row_idx, col_idx = i // cols + 1, i % cols + 1
        example_data = summary_df[summary_df['example_name'] == example]
        problem_sizes = sorted(example_data['problem_size'].unique())
        for ps_idx, problem_size in enumerate(problem_sizes):
            size_group = example_data[example_data['problem_size'] == problem_size]
            for version in ['CARTS', 'OMP']:
                ver_group = size_group[size_group['version'] == version].sort_values('threads')
                if ver_group.empty: continue

                current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
                current_dash = 'solid' if version == 'CARTS' else 'dash'
                legend_name = f"{version} (S: {problem_size})"
                show_legend_trace = legend_name not in legend_added
                if show_legend_trace: legend_added.add(legend_name)

                fig.add_trace(go.Scatter(
                    x=ver_group['threads'], y=ver_group['mean_time'], mode='lines+markers', name=legend_name,
                    line=dict(color=current_color, width=2, dash=current_dash),
                    marker=dict(size=8, color=current_color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                    legendgroup=legend_name, showlegend=show_legend_trace,
                    hovertemplate=f"<b>{example} - {version}</b><br>Size: {problem_size}<br>Threads: %{{x}}<br>Time: %{{y:.4f}} s<extra></extra>"
                ), row=row_idx, col=col_idx)
        fig.update_xaxes(title_text="Threads", type='category', row=row_idx, col=col_idx, title_standoff=5)
        fig.update_yaxes(title_text="Time (s)", type='log', row=row_idx, col=col_idx, title_standoff=5)

    calculated_height = max(500, 300 * rows)
    return template.apply(fig, plot_title="Execution Time Scaling by Example",
                          height=calculated_height, legend_title="Version (Size)",
                          xaxis_title="", yaxis_title="") # Explicitly clear global axis titles for subplots


def create_speedup_comparison_plot(summary_df, template):
    if summary_df.empty or 'speedup_vs_other' not in summary_df.columns:
        fig = go.Figure().add_annotation(text="No speedup data available", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="OMP vs CARTS Speedup", height=500)

    df_carts = summary_df[summary_df['version'] == 'CARTS'].copy()
    df_carts.dropna(subset=['speedup_vs_other'], inplace=True)
    if df_carts.empty:
        fig = go.Figure().add_annotation(text="No CARTS data with speedup to compare", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="OMP vs CARTS Speedup", height=500)

    fig = go.Figure()
    fig.add_shape(type="line", x0=0, y0=1, x1=1, y1=1, line=dict(color="gray", width=1, dash="dash"), xref="paper", yref="y")

    for (example, problem_size), group in df_carts.groupby(['example_name', 'problem_size']):
        group = group.sort_values('threads')
        if group.empty: continue

        omp_time_data = group.get('omp_time_for_comparison', pd.Series([pd.NA] * len(group)))
        customdata_stack = np.stack((
            group['problem_size'].astype(str),
            omp_time_data.apply(lambda x: format_number(x, 4) if pd.notna(x) else "N/A"),
            group['mean_time'].apply(lambda x: format_number(x, 4) if pd.notna(x) else "N/A")
        ), axis=-1)

        fig.add_trace(go.Scatter(
            x=group['threads'], y=group['speedup_vs_other'],
            mode='lines+markers', name=f"{example} (S: {problem_size})",
            line=dict(color=PRIMARY_COLOR, width=2),
            marker=dict(size=8, color=PRIMARY_COLOR, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
            customdata=customdata_stack,
            hovertemplate=(
                f"<b>{example}</b> (Size: %{{customdata[0]}})<br>" +
                "Threads: %{x}<br>" +
                "OMP/CARTS Speedup: %{y:.2f}x<br>" +
                "OMP Time: %{customdata[1]}s<br>" +
                "CARTS Time: %{customdata[2]}s<extra></extra>"
            )
        ))

    y_max_data = df_carts['speedup_vs_other'].max() if not df_carts.empty and df_carts['speedup_vs_other'].notna().any() else 2
    y_min_data = df_carts['speedup_vs_other'].min() if not df_carts.empty and df_carts['speedup_vs_other'].notna().any() else 0.5

    fig.update_layout(
        shapes=[dict(type="rect", xref="paper", yref="y", x0=0,y0=0,x1=1,y1=1,fillcolor="rgba(255,0,0,0.05)",layer="below",line_width=0),
                dict(type="rect", xref="paper", yref="y", x0=0,y0=1,x1=1,y1=max(2, y_max_data*1.1),fillcolor="rgba(0,255,0,0.05)",layer="below",line_width=0)],
        annotations=[dict(x=0.95,y=min(0.8, y_min_data*1.1 if y_min_data < 1 else 0.8),xref="paper",yref="y",text="CARTS Slower",showarrow=False,font=dict(size=12,color="darkred")),
                     dict(x=0.95,y=max(1.2, y_max_data*0.9 if y_max_data > 1 else 1.2),xref="paper",yref="y",text="CARTS Faster",showarrow=False,font=dict(size=12,color="darkgreen"))],
        xaxis=dict(type='category'))
    return template.apply(fig, plot_title="OMP vs CARTS Performance (OMP Time / CARTS Time)",
                          xaxis_title="Threads", yaxis_title="Speedup (OMP/CARTS)",
                          height=550, legend_title="Example (Size)")

def create_slowdown_comparison_plot(summary_df, template):
    if summary_df.empty:
        fig = go.Figure().add_annotation(text="No data for slowdown comparison", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CARTS vs OMP Slowdown", height=500)

    df_omp = summary_df[summary_df['version'] == 'OMP'].copy()

    if 'carts_time_for_comparison' in df_omp.columns and 'mean_time' in df_omp.columns:
        df_omp['carts_time_numeric'] = pd.to_numeric(df_omp['carts_time_for_comparison'], errors='coerce')
        df_omp['slowdown_of_carts'] = df_omp['carts_time_numeric'] / df_omp['mean_time'].replace(0, np.nan)
        df_omp.dropna(subset=['slowdown_of_carts'], inplace=True)
    else:
        fig = go.Figure().add_annotation(text="Required time columns for slowdown missing", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CARTS vs OMP Slowdown", height=500)

    if df_omp.empty:
        fig = go.Figure().add_annotation(text="No data to calculate CARTS/OMP slowdown", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CARTS vs OMP Slowdown", height=500)

    fig = go.Figure()
    fig.add_shape(type="line", x0=0, y0=1, x1=1, y1=1, line=dict(color="gray", width=1, dash="dash"), xref="paper", yref="y")

    for (example, problem_size), group in df_omp.groupby(['example_name', 'problem_size']):
        group = group.sort_values('threads')
        if group.empty: continue

        customdata_stack = np.stack((
            group['problem_size'].astype(str),
            group.get('carts_time_numeric', pd.Series([pd.NA] * len(group))).apply(lambda x: format_number(x, 4) if pd.notna(x) else "N/A"),
            group['mean_time'].apply(lambda x: format_number(x, 4) if pd.notna(x) else "N/A")
        ), axis=-1)

        fig.add_trace(go.Scatter(
            x=group['threads'], y=group['slowdown_of_carts'],
            mode='lines+markers', name=f"{example} (S: {problem_size})",
            line=dict(color=ACCENT_COLOR, width=2, dash='dash'),
            marker=dict(size=8, color=ACCENT_COLOR, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
            customdata=customdata_stack,
            hovertemplate=(
                f"<b>{example}</b> (Size: %{{customdata[0]}})<br>" +
                "Threads: %{x}<br>" +
                "CARTS/OMP Slowdown: %{y:.2f}x<br>" +
                "CARTS Time: %{customdata[1]}s<br>" +
                "OMP Time: %{customdata[2]}s<extra></extra>"
            )
        ))

    y_max_data = df_omp['slowdown_of_carts'].max() if not df_omp.empty and df_omp['slowdown_of_carts'].notna().any() else 2
    y_min_data = df_omp['slowdown_of_carts'].min() if not df_omp.empty and df_omp['slowdown_of_carts'].notna().any() else 0.5

    fig.update_layout(
        shapes=[dict(type="rect", xref="paper", yref="y", x0=0,y0=0,x1=1,y1=1,fillcolor="rgba(0,255,0,0.05)",layer="below",line_width=0),
                dict(type="rect", xref="paper", yref="y", x0=0,y0=1,x1=1,y1=max(2, y_max_data*1.1),fillcolor="rgba(255,0,0,0.05)",layer="below",line_width=0)],
        annotations=[dict(x=0.95,y=min(0.8, y_min_data*1.1 if y_min_data < 1 else 0.8),xref="paper",yref="y",text="CARTS Faster",showarrow=False,font=dict(size=12,color="darkgreen")),
                     dict(x=0.95,y=max(1.2, y_max_data*0.9 if y_max_data > 1 else 1.2),xref="paper",yref="y",text="CARTS Slower",showarrow=False,font=dict(size=12,color="darkred"))],
        xaxis=dict(type='category'))
    return template.apply(fig, plot_title="CARTS vs OMP Performance (CARTS Time / OMP Time)",
                          xaxis_title="Threads", yaxis_title="Slowdown (CARTS/OMP)",
                          height=550, legend_title="Example (Size)")


def create_parallel_efficiency_plot(summary_df, template):
    if summary_df.empty or 'parallel_efficiency' not in summary_df.columns:
        fig = go.Figure().add_annotation(text="No parallel efficiency data", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Parallel Efficiency", height=500)

    df_filtered = summary_df[summary_df['threads'] > 1].copy()
    df_filtered.dropna(subset=['parallel_efficiency'], inplace=True)
    if df_filtered.empty:
        fig = go.Figure().add_annotation(text="No data for multi-threaded efficiency", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Parallel Efficiency", height=500)

    fig = go.Figure()
    max_threads_val = df_filtered['threads'].max() if not df_filtered.empty else 0
    if pd.notna(max_threads_val) and max_threads_val >= 2:
        fig.add_trace(go.Scatter(x=list(range(2, int(max_threads_val) + 1)), y=[1.0]*(int(max_threads_val)-1),
                                 mode='lines', name='Ideal Scaling', line=dict(color='grey', dash='dash', width=1.5)))
    for (example, problem_size, version), group in df_filtered.groupby(['example_name', 'problem_size', 'version']):
        group = group.sort_values('threads')
        if group.empty: continue
        current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
        current_dash = 'solid' if version == 'CARTS' else 'dash'
        fig.add_trace(go.Scatter(x=group['threads'], y=group['parallel_efficiency'], mode='lines+markers',
                                 name=f"{example} - {version} (S: {problem_size})",
                                 line=dict(color=current_color, width=2, dash=current_dash),
                                 marker=dict(size=8, color=current_color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                                 hovertemplate=f"<b>{example} - {version}</b> (S: {problem_size})<br>Threads: %{{x}}<br>Efficiency: %{{y:.3f}}<extra></extra>"))
    max_eff = df_filtered['parallel_efficiency'].max() if not df_filtered.empty and df_filtered['parallel_efficiency'].notna().any() else 1.1
    fig.update_layout(yaxis_range=[0, max(1.1, max_eff * 1.05)], xaxis=dict(type='category'))
    return template.apply(fig, plot_title="Parallel Efficiency (Strong Scaling)", xaxis_title="Threads",
                          yaxis_title="Efficiency (T1 / (Tn * N))", height=550, legend_title="Ex - Ver (Size)")

def create_single_example_variability_plot(example_df_with_time_col, example_name, template):
    """
    Generates a single box plot for a specific example, faceted by problem size.
    Assumes example_df_with_time_col already contains the 'time' column.
    """
    if example_df_with_time_col.empty or 'time' not in example_df_with_time_col.columns:
        fig = go.Figure().add_annotation(text=f"No plottable variability data for {example_name}.", xref="paper", yref="paper", showarrow=False)
        return template.apply(fig, plot_title=f"Variability: {example_name}", height=450, xaxis_title="Threads", yaxis_title="Execution Time (s)")

    fig = px.box(example_df_with_time_col, x='threads', y='time', color='version',
                 facet_col='problem_size', facet_col_wrap=2, # Adjust wrap as needed
                 boxmode='group',
                 labels={'time': 'Execution Time (s)', 'threads': 'Threads', 'version': 'Version', 'problem_size': 'Problem Size'},
                 category_orders={'threads': sorted(example_df_with_time_col['threads'].unique())},
                 color_discrete_map={'CARTS': PRIMARY_COLOR, 'OMP': ACCENT_COLOR}
                )
    
    fig.update_yaxes(type='log', matches=None, title_standoff=10)
    fig.update_xaxes(matches=None, type='category', title_standoff=10)
    
    num_problem_sizes = example_df_with_time_col['problem_size'].nunique()
    plot_height = max(450, 250 * ((num_problem_sizes +1 )//2) )

    # Pass empty strings for global axis titles to template.apply, so subplot titles from px.box are used
    fig = template.apply(fig, plot_title=f"Execution Time Variability: {example_name}",
                         height=plot_height, legend_title="Version",
                         xaxis_title="", yaxis_title="") # Ensure px.box labels are used
    
    fig.for_each_annotation(lambda a: a.update(text=a.text.split("=")[-1])) # Clean facet titles
    fig.update_layout(margin=dict(t=60, b=120 if template.legend_y < 0 else 80))
    return fig


def create_cache_performance_comparison(profiling_df, template):
    if profiling_df.empty:
        fig = go.Figure().add_annotation(text="No cache profiling data", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Cache Performance", height=500)

    cache_miss_rate_df = profiling_df[profiling_df['event'] == 'cache-miss-rate'].copy()
    if not cache_miss_rate_df.empty:
        fig = go.Figure()
        for (example, problem_size), group_data in cache_miss_rate_df.groupby(['example_name', 'problem_size']):
            for version in ['CARTS', 'OMP']:
                ver_data = group_data[group_data['version'] == version].sort_values('threads')
                if ver_data.empty: continue
                current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
                fig.add_trace(go.Bar(
                    x=ver_data['threads'], y=ver_data['mean'] * 100, name=f"{example} (S:{problem_size}) - {version}",
                    marker_color=current_color, opacity=0.85,
                    marker_line_width=template.bar_line_width, marker_line_color=current_color,
                    hovertemplate=f"<b>{example} (S:{problem_size}) - {version}</b><br>Threads: %{{x}}<br>Cache Miss Rate: %{{y:.2f}}%<extra></extra>"
                ))
        fig.update_layout(barmode='group', xaxis_type='category')
        return template.apply(fig, plot_title="Cache Miss Rate Comparison", xaxis_title="Threads",
                              yaxis_title="Cache Miss Rate (%)", height=550, legend_title="Ex (Size) - Ver")
    else:
        cache_events_fallback = ['cache-misses', 'L1-dcache-load-misses', 'LLC-load-misses']
        df_cache_fallback = profiling_df[profiling_df['event'].isin(cache_events_fallback)].copy()
        if df_cache_fallback.empty:
            return template.apply(go.Figure().add_annotation(text="No specific cache data", x=0.5,y=0.5,showarrow=False), plot_title="Cache Performance", height=500)
        event_to_plot = df_cache_fallback['event'].unique()[0]
        df_to_plot = df_cache_fallback[df_cache_fallback['event'] == event_to_plot]
        fig = go.Figure()
        for (example, problem_size, version), group_data in df_to_plot.groupby(['example_name', 'problem_size', 'version']):
            group_data = group_data.sort_values('threads')
            current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
            current_dash = 'solid' if version == 'CARTS' else 'dash'
            fig.add_trace(go.Scatter(
                x=group_data['threads'], y=group_data['mean'], name=f"{example} (S:{problem_size}) - {version}",
                mode='lines+markers',
                line=dict(color=current_color, dash=current_dash, width=2),
                marker=dict(color=current_color, size=8, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                hovertemplate=f"<b>{example} ({version})</b><br>Threads: %{{x}}<br>{event_to_plot}: %{{y:,.0f}}<extra></extra>"
            ))
        fig.update_layout(xaxis_type='category', yaxis_type='log')
        return template.apply(fig, plot_title=f"{event_to_plot} Comparison", xaxis_title="Threads",
                              yaxis_title="Count (log)", height=550, legend_title="Ex (Size) - Ver")


def create_ipc_comparison_plot(profiling_df, template):
    if profiling_df.empty or 'IPC' not in profiling_df['event'].unique():
        fig = go.Figure().add_annotation(text="No IPC data", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="IPC Comparison", height=500)

    df_ipc = profiling_df[profiling_df['event'] == 'IPC'].copy()
    if df_ipc.empty:
        fig = go.Figure().add_annotation(text="No IPC data points", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="IPC Comparison", height=500)

    fig = go.Figure()
    for (example, problem_size, version), group in df_ipc.groupby(['example_name', 'problem_size', 'version']):
        group = group.sort_values('threads')
        if group.empty: continue
        current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
        current_dash = 'solid' if version == 'CARTS' else 'dash'
        fig.add_trace(go.Scatter(
            x=group['threads'], y=group['mean'], mode='lines+markers', name=f"{example} (S:{problem_size}) - {version}",
            line=dict(color=current_color, width=2, dash=current_dash),
            marker=dict(size=8, color=current_color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
            hovertemplate=f"<b>{example} ({version})</b><br>Size: {problem_size}<br>Threads: %{{x}}<br>IPC: %{{y:.3f}}<extra></extra>"
        ))
    fig.add_shape(type="line", x0=0,y0=1.0,x1=1,y1=1.0,line=dict(color="grey",width=1,dash="dash"),xref="paper",yref="y")
    fig.update_layout(xaxis_type='category')
    return template.apply(fig, plot_title="Instructions Per Cycle (IPC) Comparison", xaxis_title="Threads",
                          yaxis_title="IPC", height=550, legend_title="Ex (Size) - Ver")


def create_stalls_analysis_plot(profiling_df, template):
    if profiling_df.empty:
        fig = go.Figure().add_annotation(text="No stalls data", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CPU Stalls Analysis", height=500)

    stall_events_present = [e for e in profiling_df['event'].unique() if 'stall' in e.lower() or 'resource_stalls' in e.lower()]
    if not stall_events_present:
        fig = go.Figure().add_annotation(text="No stall-related events found", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CPU Stalls Analysis", height=500)

    event_to_plot = None; priority_stalls = ['cycle_activity.stalls_total', 'stalls_total', 'resource_stalls.any']
    for p_event in priority_stalls:
        if p_event in stall_events_present: event_to_plot = p_event; break
    if not event_to_plot: event_to_plot = stall_events_present[0]

    df_stalls_event = profiling_df[profiling_df['event'] == event_to_plot].copy()
    if df_stalls_event.empty:
        fig = go.Figure().add_annotation(text=f"No data for event: {event_to_plot}", x=0.5,y=0.5,showarrow=False)
        return template.apply(fig, plot_title=f"CPU Stalls: {event_to_plot}", height=500)

    fig = go.Figure(); plot_as_percentage = False
    cycles_df = profiling_df[profiling_df['event'] == 'cycles']
    if not cycles_df.empty:
        df_stalls_event = pd.merge(df_stalls_event, cycles_df[['example_name', 'problem_size', 'threads', 'version', 'mean']],
                                   on=['example_name', 'problem_size', 'threads', 'version'], suffixes=('', '_cycles'), how='left')
        if 'mean_cycles' in df_stalls_event.columns and df_stalls_event['mean_cycles'].notna().any() and (df_stalls_event['mean_cycles'] > 0).any():
            df_stalls_event['stall_percentage'] = (df_stalls_event['mean'] / df_stalls_event['mean_cycles'].replace(0, np.nan)) * 100
            plot_as_percentage = True

    y_col = 'stall_percentage' if plot_as_percentage else 'mean'
    y_title = "Stall Percentage (%)" if plot_as_percentage else f"{event_to_plot} (Count, log)"
    y_type = 'linear' if plot_as_percentage else 'log'

    for (ex, ps, ver), group in df_stalls_event.groupby(['example_name', 'problem_size', 'version']):
        group = group.sort_values('threads')
        if group.empty or y_col not in group.columns or group[y_col].isnull().all(): continue
        current_color = PRIMARY_COLOR if ver == 'CARTS' else ACCENT_COLOR
        current_dash = 'solid' if ver == 'CARTS' else 'dash'
        ht = f"<b>{ex} ({ver})</b><br>Size: {ps}<br>Threads: %{{x}}<br>"
        ht += "Stall %: %{y:.2f}%" if plot_as_percentage else f"{event_to_plot}: %{{y:,.0f}}"
        ht += "<extra></extra>"
        fig.add_trace(go.Scatter(x=group['threads'], y=group[y_col], mode='lines+markers', name=f"{ex} (S:{ps}) - {ver}",
                                 line=dict(color=current_color, width=2, dash=current_dash),
                                 marker=dict(size=8, color=current_color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                                 hovertemplate=ht))
    fig.update_layout(xaxis_type='category', yaxis_type=y_type)
    return template.apply(fig, plot_title=f"CPU Stalls: {event_to_plot}", xaxis_title="Threads",
                          yaxis_title=y_title, height=550, legend_title="Ex (Size) - Ver")


# --- Download CSV Callbacks ---
@app.callback(Output("download-benchmark-csv", "data"), Input("download-benchmark-btn", "n_clicks"),
    [State('filters-store', 'data'), State('results-store', 'data')], prevent_initial_call=True)
def download_benchmark_summary(n_clicks, filters, data_records):
    if not data_records: raise PreventUpdate
    data_manager = DataManager(data_records)
    summary_df = data_manager.get_benchmark_summary(filters)
    if summary_df.empty: raise PreventUpdate
    return dcc.send_data_frame(summary_df.to_csv, "benchmark_summary.csv", index=False)

@app.callback(Output("download-profiling-csv", "data"), Input("download-profiling-btn", "n_clicks"),
    [State('filters-store', 'data'), State('results-store', 'data')], prevent_initial_call=True)
def download_profiling_summary(n_clicks, filters, data_records):
    if not data_records: raise PreventUpdate
    data_manager = DataManager(data_records)
    # Use get_profiling_data to get categorized and derived metrics
    summary_df = data_manager.get_profiling_data(filters)
    if summary_df.empty: raise PreventUpdate("No profiling data to download after filtering.")
    return dcc.send_data_frame(summary_df.to_csv, "profiling_summary.csv", index=False)

# --- Help Modal Callbacks ---
@app.callback(
    Output({'type': 'help-modal', 'index': MATCH}, 'is_open'),
    [Input({'type': 'help-icon-btn', 'index': MATCH}, 'n_clicks'),
     Input({'type': 'close-help-modal-btn', 'index': MATCH}, 'n_clicks')],
    [State({'type': 'help-modal', 'index': MATCH}, 'is_open')],
    prevent_initial_call=True
)
def toggle_help_modal(open_clicks, close_clicks, is_open):
    # This callback handles multiple modals using pattern matching.
    # It determines which button (open or close) was clicked for which modal.
    triggered_id = ctx.triggered_id
    if not triggered_id:
        raise PreventUpdate

    # If any of the n_clicks properties are None, it means they haven't been clicked yet.
    # We only care about actual click events.
    if open_clicks is None and close_clicks is None:
        raise PreventUpdate
        
    # If an open or close button was clicked, toggle the modal's state.
    return not is_open


if __name__ == "__main__":
    app.run(debug=True)
