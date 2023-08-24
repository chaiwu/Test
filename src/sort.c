/*
 * @Author: liqing 1913282756@qq.com
 * @Date: 2023-08-24 10:59:29
 * @LastEditors: liqing 1913282756@qq.com
 * @LastEditTime: 2023-08-24 11:08:15
 * @FilePath: \sourcetreeLocalrepo\LinuxDriver\sort.c
 * @Description: 
 * 
 * Copyright (c) 2023 by liqing, All Rights Reserved. 
 */
#include "sort.h"
#include<stdbool.h>

/* 排序默认由小到大order = 1，若想反过来将最后一个参数写为 - 1 */

/* bubble sort */
void BubbleSort(int array[], int len,int order)
{
	int temp;
	bool swapped = true;
	int indexOflastUnsorted = len - 1;
	int swappedIndex = -1;
	//for(int i=0;i<len-1;i++)
	while (swapped) {
		swapped = false;
		for (int j = 0; j < indexOflastUnsorted; j++)
		{
			if (order == -1)
			{
				if (array[j] < array[j + 1])
				{
					temp = array[j];
					array[j] = array[j + 1];
					array[j + 1] = temp;
					swapped = true;
					swappedIndex = j;
				}
			}
			else
			{
				if (array[j] > array[j + 1])
				{
					temp = array[j];
					array[j] = array[j + 1];
					array[j + 1] = temp;
					swapped = true;
					swappedIndex = j;
				}
			}

		}
		indexOflastUnsorted = swappedIndex;
	}
}

/* quick sort  */
void QuickSort(int array[], int low, int high, int order)
{
	if (low >= high)
		return;
	int left = low;
	int right = high;
	int key = array[left];   /* 选第一个为分区元素 */
	while (left != right)
	{
		if (order == -1) {
			while (left < right && array[right] <= key)
				--right;
			array[left] = array[right];
			while (left < right && array[left] >= key)
				++left;
			array[right] = array[left];
		}
		else
		{
			while (left < right && array[right] >= key)
				--right;
			array[left] = array[right];
			while (left < right && array[left] <= key)
				++left;
			array[right] = array[left];
		}
		
	}
	array[left] = key;
	QuickSort(array, low, left - 1, order);
	QuickSort(array, left+1, high, order);
}

/* insert sort */
void InsertSort(int array[],int len,int order)
{
	int j;
	for (int i = 1; i < len; i++)
	{
		int x = array[i];
		for (j = i; j >= 0; j--)
			if (order == -1)
			{
				if (x > array[j - 1])
					array[j] = array[j - 1];
				else
					break;
			}
			else
			{
				if (x < array[j - 1])
					array[j] = array[j - 1];
				else
					break;
			}
		array[j] = x;
	}

}

/* shell sort  */
void ShellSort(int array[],int len,int d,int order)
{
	for (int inc = d; inc > 0; inc /= 2) {        //循环的次数为增量缩小至1的次数
		for (int i = inc; i < len; ++i) {       //循环的次数为第一个分组的第二个元素到数组的结束
			int j = i - inc;
			int temp = array[i];
			if (order == -1)
			{
				while (j >= 0 && array[j] < temp)
				{
					array[j + inc] = array[j];
					j = j - inc;
				}
			}
			else
			{
				while (j >= 0 && array[j] > temp)
				{
					array[j + inc] = array[j];
					j = j - inc;
				}
			}
			
			if ((j + inc) != i)//防止自我插入
				array[j + inc] = temp;//插入记录
		}
	}

}

/* merge sort */
void merge(int array[], int temp[], int l, int r, int order)
{
	if (l >= r) return;
	int mid = (l + r) / 2;
	merge(array,temp, l, mid, order);
	merge(array, temp,mid + 1, r, order);
	// 归并的过程
	int k = 0, i = l, j = mid + 1;
	while (i <= mid && j <= r)
		if (order == -1)
		{
			if (array[i] >= array[j]) temp[k++] = array[i++];
			else temp[k++] = array[j++];
		}
		else
		{
			if (array[i] <= array[j]) temp[k++] = array[i++];
			else temp[k++] = array[j++];
		}
		

	while (i <= mid) temp[k++] = array[i++];
	while (j <= r) temp[k++] = array[j++];

	for (i = l, j = 0; i <= r; i++, j++) array[i] = temp[j];
}


void MergeSort(int array[],int temp[], int l, int r, int order)
{
	//int* temp = new int[sizeof(array) / sizeof(array[0]) + 1];
	merge(array, temp, l, r, order);
	//delete[]temp;
}

/*  heap sort  */
void HeapSort()
{

}

/*  selection sort  */
void SelectionSort(int array[],int len,int order)
{
	int i, j, k;
	for (i = 0; i < len; i++) {
		k = i;
		for (j = i + 1; j < len; j++) {
			if (order == -1)
			{
				if (array[j] > array[k])
					k = j;
			}
			else
			{
				if (array[j] < array[k])
					k = j;
			}
			
		}
		if (i != k) {
			array[i] = array[i] + array[k];
			array[k] = array[i] - array[k];
			array[i] = array[i] - array[k];
		}
	}
}

