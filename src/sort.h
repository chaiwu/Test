#ifndef __SORT__
#define __SORT__
/* 排序由小到大 order = 1，若想反过来将order = - 1 */

/* bubble sort */
void BubbleSort(int array[], int len, int order);

/* quick sort  */
void QuickSort(int array[], int low, int high, int order);

/* insert sort */
void InsertSort(int array[], int len, int order);

/* shell sort  */
void ShellSort(int array[], int len, int d, int order);

/* merge sort */
void MergeSort(int array[], int temp[], int l, int r, int order);

/*  heap sort  */
void HeapSort();

/*  selection sort  */
void SelectionSort(int array[], int len, int order);

#endif
